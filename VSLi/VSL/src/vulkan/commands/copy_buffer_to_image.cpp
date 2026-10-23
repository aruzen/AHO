//
// Created by morimoto_hibiki on 2025/10/22.
//

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>

#include "../image_format.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
    void validate_upload(const VSL_NAMESPACE::ImageAccessor& image, const std::shared_ptr<VSL_NAMESPACE::impl::BufferImpl>& source,
                         VSL_NAMESPACE::ImageLayout layout, std::uint32_t layer,
                         const VSL_NAMESPACE::CommandBuffer& commands, const VSL_NAMESPACE::CommandManager& manager) {
        const auto& destination = image.data_;
        if (!source || !source->buffer || !destination || !destination->image || !destination->device ||
            source->device != destination->device || !commands.data_ || !commands.data_->command_pool || !manager.data_ ||
            commands.data_->command_pool->device != destination->device || manager.data_->device != destination->device)
            throw std::invalid_argument("Image upload requires valid resources and commands on the same device");
        if (!VSL_NAMESPACE::contain(source->usage, VSL_NAMESPACE::MemoryType::TRANSFER_SOURCE) ||
            !VSL_NAMESPACE::contain(destination->usage, VSL_NAMESPACE::ImageType::TRANSFER_DST))
            throw std::invalid_argument("Image upload requires TransferSource buffer and TransferDst image usage");
        if (layout != VSL_NAMESPACE::ImageLayout::GENERAL && layout != VSL_NAMESPACE::ImageLayout::TRANSFER_DST_OPTIMAL)
            throw std::invalid_argument("Image upload requires General or TransferDstOptimal layout");
        if (destination->samples != VSL_NAMESPACE::SampleCount::ONE ||
            VSL_NAMESPACE::impl::helper::image_aspects(destination->format) != vk::ImageAspectFlagBits::eColor)
            throw std::invalid_argument("Image upload requires a single-sample color image");
        if (!destination->width || !destination->height || layer >= destination->count)
            throw std::out_of_range("Image upload extent or array layer is outside image storage");
        const auto texel_size = VSL_NAMESPACE::data_format::convert(static_cast<VSL_NAMESPACE::data_format::FormatTypes>(destination->format)).size();
        if (!texel_size)
            throw std::invalid_argument("Image upload format has no supported texel size");
        const auto row_size = std::uint64_t(destination->width) * texel_size;
        if (row_size > std::numeric_limits<std::size_t>::max() / destination->height)
            throw std::length_error("Image upload byte count overflow");
        if (row_size * destination->height > source->requirement_size)
            throw std::out_of_range("Image upload exceeds source buffer");
    }
} // namespace

VSL_NAMESPACE::command::CopyBufferToImage::CopyBufferToImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                             VSL_NAMESPACE::ImageLayout layout)
    : image(std::move(image)), buffer(buffer), layout(layout), _source(buffer ? buffer->data_ : nullptr) {}

void VSL_NAMESPACE::command::CopyBufferToImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    validate_upload(image, _source, layout, 0, buffer, manager);
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
    validate_upload(image, _source, layout, target_idx, buffer, manager);
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
