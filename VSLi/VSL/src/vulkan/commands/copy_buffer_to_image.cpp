//
// Created by morimoto_hibiki on 2025/10/22.
//

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>

#include <utility>

VSL_NAMESPACE::command::CopyBufferToImage::CopyBufferToImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                             VSL_NAMESPACE::ImageLayout layout)
    : image(std::move(image)), buffer(buffer), layout(layout), _source(buffer ? buffer->data_ : nullptr) {}

void VSL_NAMESPACE::command::CopyBufferToImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    if (!_source || !VSL_NAMESPACE::contain(_source->usage, VSL_NAMESPACE::MemoryType::TRANSFER_SOURCE))
        throw std::invalid_argument("Image upload requires TransferSource buffer usage");
    buffer.retain(image.data_);
    buffer.retain(_source);
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0; // tightly packed
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{image.data_->width, image.data_->height, 1};

    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].copyBufferToImage(
        _source->buffer,
        this->image.data_->image,
        (vk::ImageLayout)layout,
        region);
}

VSL_NAMESPACE::command::CopyBufferToArrayImage::CopyBufferToArrayImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                                       VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx)
    : image(std::move(image)), buffer(buffer), layout(layout), target_idx(target_idx), _source(buffer ? buffer->data_ : nullptr) {}

void VSL_NAMESPACE::command::CopyBufferToArrayImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    if (!_source || !VSL_NAMESPACE::contain(_source->usage, VSL_NAMESPACE::MemoryType::TRANSFER_SOURCE))
        throw std::invalid_argument("Image upload requires TransferSource buffer usage");
    buffer.retain(image.data_);
    buffer.retain(_source);
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0; // tightly packed
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = target_idx;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{image.data_->width, image.data_->height, 1};

    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].copyBufferToImage(
        _source->buffer,
        this->image.data_->image,
        (vk::ImageLayout)layout,
        region);
}
