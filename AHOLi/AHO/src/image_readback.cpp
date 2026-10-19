#include <AHO/image_readback.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE {
    struct ImageReadback::Data {
        VSL_NAMESPACE::CommandManager manager;
        std::optional<VSL_NAMESPACE::CommandPool> pool;
        std::optional<VSL_NAMESPACE::CommandBuffer> commandBuffer;
        vk::UniqueFence fence;
        std::shared_ptr<VSL_NAMESPACE::_impl::Buffer_impl> buffer;
        VSL_NAMESPACE::ImageAccessor source;
        VSL_NAMESPACE::command::ImageCopyInfo info;
        vk::DeviceSize atomSize = 1;
        void* mapped = nullptr;
        bool submitted = false, coherent = false;

        explicit Data(VSL_NAMESPACE::CommandManager manager) : manager(std::move(manager)) {}
        ~Data() {
            auto device = manager._data->device->device;
            if (submitted)
                (void)device.waitForFences(1, &*fence, true, UINT64_MAX);
            if (mapped)
                device.unmapMemory(buffer->deviceMem);
            commandBuffer.reset();
            if (pool)
                device.destroyCommandPool(pool->_data->commandPool);
        }

        void reserve(std::size_t bytes) {
            if (!bytes || (buffer && buffer->requirementSize >= bytes))
                return;
            auto owner = manager._data->device;
            auto device = owner->device;
            auto next = std::make_shared<VSL_NAMESPACE::_impl::Buffer_impl>();
            next->device = owner;
            next->requirementSize = bytes;
            next->buffer = device.createBuffer({.size = bytes,
                                                .usage = vk::BufferUsageFlagBits::eTransferDst,
                                                .sharingMode = vk::SharingMode::eExclusive});
            const auto requirements = device.getBufferMemoryRequirements(next->buffer);
            const auto properties = owner->parentDevice->device.getMemoryProperties();
            std::optional<std::uint32_t> memoryType;
            bool nextCoherent = false;
            for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
                const auto flags = properties.memoryTypes[i].propertyFlags;
                if (!(requirements.memoryTypeBits & (1u << i)) || !(flags & vk::MemoryPropertyFlagBits::eHostVisible))
                    continue;
                memoryType = i;
                nextCoherent = static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eHostCoherent);
                if (nextCoherent)
                    break;
            }
            if (!memoryType)
                throw std::runtime_error("No host-visible readback memory type");
            next->allocatedSize = requirements.size;
            next->deviceMem = device.allocateMemory({.allocationSize = requirements.size, .memoryTypeIndex = *memoryType});
            device.bindBufferMemory(next->buffer, next->deviceMem, 0);
            void* nextMapped = device.mapMemory(next->deviceMem, 0, VK_WHOLE_SIZE);
            if (mapped)
                device.unmapMemory(buffer->deviceMem);
            buffer = std::move(next);
            mapped = nextMapped;
            coherent = nextCoherent;
            if (!coherent)
                atomSize = owner->parentDevice->device.getProperties().limits.nonCoherentAtomSize;
        }
    };

    ImageReadback::ImageReadback(VSL_NAMESPACE::CommandManager manager, std::size_t initialCapacity) {
        if (!manager._data || !manager._data->device || !manager._data->device->device)
            throw std::invalid_argument("Readback requires a valid command manager");
        auto data = std::make_shared<Data>(std::move(manager));
        data->pool.emplace(VSL_NAMESPACE::LogicalDeviceAccessor{data->manager._data->device});
        data->commandBuffer.emplace(*data->pool, 1);
        data->fence = data->manager._data->device->device.createFenceUnique({});
        data->reserve(initialCapacity);
        _data = std::move(data);
    }

    ImageReadback::ImageReadback(VSL_NAMESPACE::CommandManager manager, VSL_NAMESPACE::ImageAccessor image,
                                 VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region)
        : ImageReadback(std::move(manager)) {
        capture(std::move(image), layout, region);
    }

    bool ImageReadback::capture(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region) {
        if (!_data)
            throw std::logic_error("Readback handle is empty");
        const auto info = VSL_NAMESPACE::command::ImageReadback::describe(image, layout, region);
        auto& data = *_data;
        if (image._data->device != data.manager._data->device)
            throw std::invalid_argument("Readback requires an image on the manager's device");
        if (data.submitted && !ready())
            return false;
        data.reserve(info.byteSize);
        data.submitted = false;
        data.source = std::move(image);
        data.info = info;
        auto command = data.commandBuffer->_data->commandBuffers.front();
        command.reset();
        command.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        VSL_NAMESPACE::command::ImageReadback(data.buffer, data.source, layout, info.region)
            .invoke(*data.pool, *data.commandBuffer, data.manager);
        command.end();
        auto device = data.manager._data->device->device;
        device.resetFences(*data.fence);
        data.manager._data->graphicsQueue.submit(
            vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &command}, *data.fence);
        data.submitted = true;
        return true;
    }

    bool ImageReadback::ready() const {
        return _data && _data->submitted &&
               _data->manager._data->device->device.getFenceStatus(*_data->fence) == vk::Result::eSuccess;
    }

    bool ImageReadback::wait(std::uint64_t timeout) const {
        if (!_data || !_data->submitted)
            throw std::logic_error("Readback has no submitted capture");
        return _data->manager._data->device->device.waitForFences(*_data->fence, true, timeout) == vk::Result::eSuccess;
    }

    ImagePixels ImageReadback::read(std::uint64_t timeout) const {
        ImagePixels result;
        read(result, timeout);
        return result;
    }

    void ImageReadback::read(ImagePixels& destination, std::uint64_t timeout) const {
        if (!wait(timeout))
            throw std::runtime_error("Readback timed out");
        const auto& data = *_data;
        destination.bytes.resize(data.info.byteSize);
        if (!data.coherent) {
            const vk::DeviceSize bytes = data.info.byteSize;
            const auto padding = (data.atomSize - bytes % data.atomSize) % data.atomSize;
            const auto size = data.buffer->allocatedSize - bytes < padding ? VK_WHOLE_SIZE : bytes + padding;
            data.manager._data->device->device.invalidateMappedMemoryRanges(
                vk::MappedMemoryRange{.memory = data.buffer->deviceMem, .offset = 0, .size = size});
        }
        std::memcpy(destination.bytes.data(), data.mapped, destination.bytes.size());
        destination.width = data.info.region.width;
        destination.height = data.info.region.height;
        destination.rowStride = data.info.rowStride;
        destination.format = data.info.format;
    }

    std::size_t ImageReadback::capacity() const {
        return _data && _data->buffer ? _data->buffer->requirementSize : 0;
    }
} // namespace AHO_NAMESPACE
