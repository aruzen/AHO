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
    VSL_NAMESPACE::command::ImageCopyInfo describe_upload(
        const VSL_NAMESPACE::ImageAccessor& image, const std::shared_ptr<VSL_NAMESPACE::impl::BufferImpl>& source,
        VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region, std::size_t offset) {
        const auto& destination = image.data_;
        if (!source || !source->buffer || !destination || !destination->image || !destination->device ||
            source->device != destination->device)
            throw std::invalid_argument("Image upload requires valid resources on the same device");
        if (!VSL_NAMESPACE::contain(source->usage, VSL_NAMESPACE::MemoryType::TRANSFER_SOURCE) ||
            !VSL_NAMESPACE::contain(destination->usage, VSL_NAMESPACE::ImageType::TRANSFER_DST))
            throw std::invalid_argument("Image upload requires TransferSource buffer and TransferDst image usage");
        if (layout != VSL_NAMESPACE::ImageLayout::GENERAL && layout != VSL_NAMESPACE::ImageLayout::TRANSFER_DST_OPTIMAL)
            throw std::invalid_argument("Image upload requires General or TransferDstOptimal layout");
        if (destination->samples != VSL_NAMESPACE::SampleCount::ONE ||
            VSL_NAMESPACE::impl::helper::image_aspects(destination->format) != vk::ImageAspectFlagBits::eColor)
            throw std::invalid_argument("Image upload requires a single-sample color image");
        if (!region.width && !region.height) {
            if (region.x || region.y)
                throw std::invalid_argument("Full-image upload requires origin zero");
            region.width = destination->width;
            region.height = destination->height;
        }
        if (!region.width || !region.height || region.x >= destination->width || region.y >= destination->height ||
            region.width > destination->width - region.x || region.height > destination->height - region.y ||
            region.layer >= destination->count || region.x > std::numeric_limits<std::int32_t>::max() ||
            region.y > std::numeric_limits<std::int32_t>::max())
            throw std::out_of_range("Image upload region is outside image storage");
        const auto texel_size = VSL_NAMESPACE::data_format::convert(static_cast<VSL_NAMESPACE::data_format::FormatTypes>(destination->format)).size();
        if (!texel_size)
            throw std::invalid_argument("Image upload format has no supported texel size");
        const auto row_size = std::uint64_t(region.width) * texel_size;
        if (row_size > std::numeric_limits<std::size_t>::max() / region.height)
            throw std::length_error("Image upload byte count overflow");
        const auto byte_size = static_cast<std::size_t>(row_size * region.height);
        if (offset % 4 || offset % texel_size)
            throw std::invalid_argument("Image upload offset must align to four bytes and a texel");
        if (offset > source->requirement_size || byte_size > source->requirement_size - offset)
            throw std::out_of_range("Image upload exceeds source buffer");
        return {region, image.get_format(), static_cast<std::size_t>(row_size), byte_size};
    }

    void validate_commands(const VSL_NAMESPACE::ImageAccessor& image, const VSL_NAMESPACE::CommandBuffer& commands,
                           const VSL_NAMESPACE::CommandManager& manager) {
        const auto& device = image.data_->device;
        if (!commands.data_ || !commands.data_->command_pool || !manager.data_ ||
            commands.data_->command_pool->device != device || manager.data_->device != device)
            throw std::invalid_argument("Image upload requires commands on the same device");
    }

    void record_upload(const VSL_NAMESPACE::ImageAccessor& image, const std::shared_ptr<VSL_NAMESPACE::impl::BufferImpl>& source,
                       VSL_NAMESPACE::ImageLayout layout, const VSL_NAMESPACE::command::ImageCopyInfo& info,
                       std::size_t offset, VSL_NAMESPACE::CommandBuffer buffer) {
        buffer.retain(image.data_);
        buffer.retain(source);
        vk::BufferImageCopy region;
        region.bufferOffset = offset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = info.region.layer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{static_cast<std::int32_t>(info.region.x), static_cast<std::int32_t>(info.region.y), 0};
        region.imageExtent = vk::Extent3D{info.region.width, info.region.height, 1};
        buffer.data_->command_buffers.at(buffer.get_current_buffer_idx()).copyBufferToImage(source->buffer, image.data_->image, static_cast<vk::ImageLayout>(layout), region);
    }
} // namespace

VSL_NAMESPACE::command::CopyBufferToImage::CopyBufferToImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                             VSL_NAMESPACE::ImageLayout layout)
    : CopyBufferToImage(std::move(image), buffer, layout, {}, 0) {}

VSL_NAMESPACE::command::CopyBufferToImage::CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer,
                                                             ImageLayout layout, ImageRegion region, std::size_t offset)
    : layout(layout), image(std::move(image)), buffer(buffer), region(region), offset(offset),
      _source(buffer ? buffer->data_ : nullptr) {}

VSL_NAMESPACE::command::ImageCopyInfo VSL_NAMESPACE::command::CopyBufferToImage::description() const {
    return describe_upload(image, _source, layout, region, offset);
}

void VSL_NAMESPACE::command::CopyBufferToImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    const auto info = description();
    validate_commands(image, buffer, manager);
    record_upload(image, _source, layout, info, offset, buffer);
}

VSL_NAMESPACE::command::CopyBufferToArrayImage::CopyBufferToArrayImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                                       VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx)
    : image(std::move(image)), buffer(buffer), layout(layout), target_idx(target_idx), _source(buffer ? buffer->data_ : nullptr) {}

void VSL_NAMESPACE::command::CopyBufferToArrayImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    const auto info = describe_upload(image, _source, layout, ImageRegion{.layer = target_idx}, 0);
    validate_commands(image, buffer, manager);
    record_upload(image, _source, layout, info, 0, buffer);
}
