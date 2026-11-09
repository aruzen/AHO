//
// Created by morimoto_hibiki on 2025/10/22.
//

#include <VSL/lifetime/command_access.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/resource_use_factory.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/vulkan/resource_use.hpp>

#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>

#include "../image_format.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    VSL_NAMESPACE::command::ImageCopyInfo describe_upload(
        const VSL_NAMESPACE::ImageAccessor& image, const VSL_NAMESPACE::impl::BufferImpl* source,
        VSL_NAMESPACE::ImageLayout layout, VSL_NAMESPACE::ImageRegion region, std::size_t offset,
        std::size_t source_offset, std::size_t source_bytes) {
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
        if (source_offset > source->requirement_size || source_bytes > source->requirement_size - source_offset ||
            offset > source_bytes || byte_size > source_bytes - offset)
            throw std::out_of_range("Image upload exceeds source buffer view");
        const auto native_offset = source_offset + offset;
        if (native_offset % 4 || native_offset % texel_size)
            throw std::invalid_argument("Image upload offset must align to four bytes and a texel");
        return {region, image.get_format(), static_cast<std::size_t>(row_size), byte_size};
    }

    void validate_commands(const VSL_NAMESPACE::ImageAccessor& image, const VSL_NAMESPACE::CommandBuffer& commands,
                           const VSL_NAMESPACE::CommandManager& manager) {
        const auto& device = image.data_->device;
        if (!CommandAccess::Data(commands) || !CommandAccess::Data(commands)->command_pool || !CommandAccess::Data(manager) ||
            CommandAccess::Data(commands)->command_pool->device != device || CommandAccess::Data(manager)->device.get() != device)
            throw std::invalid_argument("Image upload requires commands on the same device");
    }

    void record_upload(const VSL_NAMESPACE::ImageAccessor& image, const VSL_NAMESPACE::impl::BufferImpl& source,
                       VSL_NAMESPACE::ImageLayout layout, const VSL_NAMESPACE::command::ImageCopyInfo& info,
                       std::size_t offset, VSL_NAMESPACE::CommandBuffer buffer) {
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
        CommandAccess::Data(buffer)->command_buffers.at(buffer.get_current_buffer_idx()).copyBufferToImage(source.buffer, image.data_->image, static_cast<vk::ImageLayout>(layout), region);
    }
} // namespace

VSL_NAMESPACE::command::CopyBufferToImage::CopyBufferToImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                             VSL_NAMESPACE::ImageLayout layout)
    : CopyBufferToImage(std::move(image), buffer, layout, {}, 0) {}

VSL_NAMESPACE::command::CopyBufferToImage::CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer,
                                                             ImageLayout layout, ImageRegion region, std::size_t offset)
    : layout(layout), image(std::move(image)), buffer(buffer), region(region), offset(offset),
      _source(buffer ? buffer->data_.get() : nullptr), _use(buffer ? buffer->resource_use() : ResourceUse{}),
      _source_bytes(buffer ? buffer->requested_size() : 0) {}

VSL_NAMESPACE::command::CopyBufferToImage::CopyBufferToImage(ImageAccessor image, BufferView source,
                                                             ImageLayout layout, ImageRegion region, std::size_t offset,
                                                             ResourceUse constraint, ResourceRetention retention)
    : layout(layout), image(std::move(image)), buffer(nullptr), region(region), offset(offset),
      _source(lifetime::detail::BufferAccess::Impl(source.ref())), _use(source.resource_use(std::move(constraint))),
      _source_offset(source.byte_offset()), _source_bytes(source.size()), _retention(retention) {}

VSL_NAMESPACE::command::ImageCopyInfo VSL_NAMESPACE::command::CopyBufferToImage::description() const {
    return describe_upload(image, _source, layout, region, offset, _source_offset, _source_bytes);
}

VSL_NAMESPACE::command::ImageCopyInfo VSL_NAMESPACE::command::CopyBufferToImage::prepare(CommandBuffer buffer, const CommandManager& manager) const {
    const auto info = description();
    validate_commands(image, buffer, manager);
    const auto completion = buffer.completion();
    const auto slot = buffer.get_current_buffer_idx();
    const auto context = CommandAccess::Data(buffer)->recording_context;
    if (!completion || completion.status() != Completion::Status::PENDING)
        throw std::logic_error("Image upload requires a recording phase");
    _use.record(buffer, _retention);
    image.resource_use().record(buffer);
    if (buffer.completion() != completion || buffer.get_current_buffer_idx() != slot || CommandAccess::Data(buffer)->recording_context != context ||
        completion.status() != Completion::Status::PENDING)
        throw std::logic_error("Image upload recording changed during resource registration");
    return info;
}

void VSL_NAMESPACE::command::CopyBufferToImage::record(const ImageCopyInfo& info, const CommandBuffer& buffer) const {
    record_upload(image, *_source, layout, info, _source_offset + offset, buffer);
}

void VSL_NAMESPACE::command::CopyBufferToImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    const auto copy = *this;
    const auto info = copy.prepare(buffer, manager);
    copy.record(info, buffer);
}

VSL_NAMESPACE::command::CopyBufferToArrayImage::CopyBufferToArrayImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                                       VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx)
    : image(std::move(image)), buffer(buffer), layout(layout), target_idx(target_idx), _source(buffer ? buffer->data_ : nullptr) {}

void VSL_NAMESPACE::command::CopyBufferToArrayImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    const auto copy = *this;
    if (!copy._source)
        throw std::invalid_argument("Image upload requires a source buffer");
    const auto source = lifetime::detail::BufferAccess::View(lifetime::detail::BufferAccess::Ref(copy._source.get()),
                                                             copy._source, 0, copy._source->requirement_size);
    CopyBufferToImage(copy.image, source, copy.layout, ImageRegion{.layer = copy.target_idx}, 0, {}, ResourceRetention::SUBMISSION)
        .invoke(pool, buffer, manager);
}
