#include <VSL/vulkan/image_readback.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>
#include "image_format.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>

VSL_NAMESPACE::_impl::ImageReadback_impl::~ImageReadback_impl() {
    if (!device)
        return;
    if (submitted)
        (void)device->device.waitForFences(1, &fence, true, UINT64_MAX);
    commandBuffer.reset();
    device->device.destroyFence(fence);
    device->device.destroyCommandPool(pool);
}
namespace vsl {
    ImageReadback::ImageReadback(CommandManager manager, ImageAccessor image, ImageLayout layout,
                                 ImageRegion region) {
        if (!manager._data || !image._data || !image._data->image || manager._data->device != image._data->device)
            throw std::invalid_argument("Readback requires an image and manager on the same device");
        const auto &src = *image._data;
        if (!contain(src.usage, ImageType::TransferSrc) || src.samples != SampleCount::One ||
            _impl::helper::imageAspects(src.format) != vk::ImageAspectFlagBits::eColor)
            throw std::invalid_argument("Readback requires a single-sample color TransferSrc image");
        if (layout == ImageLayout::Undefined || layout == ImageLayout::Preinitialized ||
            layout == ImageLayout::DepthStencilAttachmentOptimal ||
            layout == ImageLayout::DepthStencilReadOnlyOptimal)
            throw std::invalid_argument("Readback requires defined color image contents");
        if (!region.width && !region.height) {
            if (region.x || region.y)
                throw std::invalid_argument("Full-image readback requires origin zero");
            region.width = src.width;
            region.height = src.height;
        }
        if (!region.width || !region.height || region.x >= src.width || region.y >= src.height ||
            region.width > src.width - region.x || region.height > src.height - region.y ||
            region.layer >= src.count)
            throw std::out_of_range("Readback region exceeds image");
        const auto format = data_format::convert(static_cast<data_format::___FormatTypes>(src.format));
        if (!format.size())
            throw std::invalid_argument("Readback format has no supported texel size");
        const std::uint64_t row = std::uint64_t(region.width) * format.size();
        if (row > std::numeric_limits<std::size_t>::max() / region.height)
            throw std::length_error("Readback byte count overflow");
        const auto bytes = static_cast<std::size_t>(row * region.height);
        auto data = std::make_shared<_impl::ImageReadback_impl>();
        data->device = manager._data->device;
        data->source = image._data;
        data->pixels.width = region.width;
        data->pixels.height = region.height;
        data->pixels.rowStride = static_cast<std::size_t>(row);
        data->pixels.format = format;
        data->buffer = std::make_shared<_impl::Buffer_impl>();
        data->buffer->device = data->device;
        data->buffer->requirementSize = bytes;
        auto device = data->device->device;
        vk::BufferCreateInfo bufferInfo{.size = bytes,
                                        .usage = vk::BufferUsageFlagBits::eTransferDst,
                                        .sharingMode = vk::SharingMode::eExclusive};
        data->buffer->buffer = device.createBuffer(bufferInfo);
        const auto requirements = device.getBufferMemoryRequirements(data->buffer->buffer);
        const auto properties = data->device->parentDevice->device.getMemoryProperties();
        std::optional<std::uint32_t> memoryType;
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            const auto flags = properties.memoryTypes[i].propertyFlags;
            if (!(requirements.memoryTypeBits & (1u << i)) || !(flags & vk::MemoryPropertyFlagBits::eHostVisible))
                continue;
            memoryType = i;
            data->coherent = static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eHostCoherent);
            if (data->coherent)
                break;
        }
        if (!memoryType)
            throw std::runtime_error("No host-visible readback memory type");
        data->buffer->allocatedSize = requirements.size;
        data->buffer->deviceMem =
            device.allocateMemory({.allocationSize = requirements.size, .memoryTypeIndex = *memoryType});
        device.bindBufferMemory(data->buffer->buffer, data->buffer->deviceMem, 0);
        data->pool = device.createCommandPool({.queueFamilyIndex = data->device->graphicsFamily});
        auto poolData = std::make_shared<_impl::CommandPool_impl>();
        poolData->device = data->device;
        poolData->commandPool = data->pool;
        CommandPool pool(poolData);
        CommandBuffer commandBuffer(pool, 1);
        data->commandBuffer = commandBuffer._data;
        auto command = commandBuffer._data->commandBuffers.front();
        data->fence = device.createFence({});
        command.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        command::ChangeImageBarrier(image, layout, ImageLayout::TransferSrcOptimal, region.layer, 1)
            .invoke(pool, commandBuffer, manager);
        command::CopyImageToBuffer(data->buffer, image, ImageLayout::TransferSrcOptimal, region, true)
            .invoke(pool, commandBuffer, manager);
        command::ChangeImageBarrier(image, ImageLayout::TransferSrcOptimal, layout, region.layer, 1)
            .invoke(pool, commandBuffer, manager);
        command.end();
        vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &command};
        manager._data->graphicsQueue.submit(submit, data->fence);
        data->submitted = true;
        data_ = std::move(data);
    }
    bool ImageReadback::ready() const {
        if (!data_)
            throw std::logic_error("Readback handle is empty");
        return data_->device->device.getFenceStatus(data_->fence) == vk::Result::eSuccess;
    }
    bool ImageReadback::wait(std::uint64_t timeout) const {
        if (!data_)
            throw std::logic_error("Readback handle is empty");
        return data_->device->device.waitForFences(data_->fence, true, timeout) == vk::Result::eSuccess;
    }
    ImagePixels ImageReadback::read(std::uint64_t timeout) const {
        if (!wait(timeout))
            throw std::runtime_error("Readback timed out");
        auto result = data_->pixels;
        result.bytes.resize(data_->buffer->requirementSize);
        auto device = data_->device->device;
        auto memory = data_->buffer->deviceMem;
        const auto mapped = device.mapMemory(memory, 0, VK_WHOLE_SIZE);
        try {
            if (!data_->coherent)
                device.invalidateMappedMemoryRanges(
                    vk::MappedMemoryRange{.memory = memory, .offset = 0, .size = VK_WHOLE_SIZE});
            std::memcpy(result.bytes.data(), mapped, result.bytes.size());
        } catch (...) {
            device.unmapMemory(memory);
            throw;
        }
        device.unmapMemory(memory);
        return result;
    }
} // namespace vsl
