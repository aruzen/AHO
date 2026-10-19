#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/image.hpp>

#include <utility>
#include <stdexcept>
#include "image_format.hpp"
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>

size_t VSL_NAMESPACE::ImageAccessor::size() {
    return std::size_t(_data->width) * _data->height * _data->count * convert((data_format::___FormatTypes)_data->format).size();
}

std::shared_ptr<VSL_NAMESPACE::_impl::Image_impl>
VSL_NAMESPACE::ImageAccessor::MakeImage(VSL_NAMESPACE::ImageType type,
                                        VSL_NAMESPACE::MemoryProperty prop,
                                        VSL_NAMESPACE::ImageLayout layout,
                                        VSL_NAMESPACE::LogicalDeviceAccessor device,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        VSL_NAMESPACE::data_format::___Format format,
                                        std::uint32_t count, SampleCount samples) {
    if (!device._data || !width || !height || !count || format.type() == data_format::___FormatTypes::Undefined)
        throw std::invalid_argument("Invalid image device/extent/format");
    const auto sampleBits = static_cast<std::uint32_t>(samples);
    if (!sampleBits || sampleBits > 64 || (sampleBits & (sampleBits - 1)))
        throw std::invalid_argument("Invalid image sample count");
    auto _data = std::make_shared<_impl::Image_impl>();
    _data->device = device._data;
    _data->height = height;
    _data->width = width;
    _data->count = count;
    _data->usage = type;
    _data->samples = samples;
    _data->format = (vk::Format)format.type();

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = _data->format;
    imageInfo.extent = vk::Extent3D{width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = count;
    imageInfo.samples = static_cast<vk::SampleCountFlagBits>(samples);
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = (vk::ImageUsageFlagBits)type;
    imageInfo.initialLayout = (vk::ImageLayout)layout;

    _data->image = device._data->device.createImage(imageInfo);

    vk::MemoryRequirements memRequirements = device._data->device.getImageMemoryRequirements(_data->image);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VSL_NAMESPACE::_impl::helper::findMemoryType(device._data, memRequirements.memoryTypeBits,
                                                                             (vk::MemoryPropertyFlagBits)prop);

    _data->memory = device._data->device.allocateMemory(allocInfo);

    device._data->device.bindImageMemory(_data->image, _data->memory, 0);

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = _data->image;
    viewInfo.viewType = count == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
    viewInfo.format = (vk::Format)format.type();
    ;
    viewInfo.subresourceRange.aspectMask = _impl::helper::imageAspects(_data->format);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = count;

    _data->view = device._data->device.createImageView(viewInfo);
    return _data;
}

void VSL_NAMESPACE::ImageAccessor::TransformLayout(VSL_NAMESPACE::CommandManager manager, std::shared_ptr<_impl::Image_impl> data,
                                                   VSL_NAMESPACE::ImageLayout oldLayout, VSL_NAMESPACE::ImageLayout newLayout) {
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::ChangeImageBarrier(ImageAccessor{std::move(data)}, oldLayout, newLayout);
}

bool VSL_NAMESPACE::ImageAccessor::copyByBuffer(CommandManager manager, BufferAccessor* buf, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx) {
    auto phase = manager.startPhase<ComputePhase>();
    if (this->_data->count == 1)
        phase << command::CopyBufferToImage(*this, buf, layout);
    else
        phase << command::CopyBufferToArrayImage(*this, buf, layout, target_idx);
    return true;
}

VSL_NAMESPACE::_impl::Image_impl::~Image_impl() {
    if (!device)
        return;
    device->device.destroy(view);
    if (memory != VK_NULL_HANDLE) {
        device->device.destroy(image);
        device->device.free(memory);
    }
}
VSL_NAMESPACE::data_format::___Format VSL_NAMESPACE::ImageAccessor::getFormat() const {
    const auto type = static_cast<data_format::___FormatTypes>(_data->format);
    const auto format = data_format::convert(type);
    return format.type() == data_format::___FormatTypes::Undefined ? data_format::___Format(type, 0) : format;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::ImageAccessor::getDevice() const {
    if (!_data)
        throw std::invalid_argument("Image is empty");
    return {_data->device};
}
