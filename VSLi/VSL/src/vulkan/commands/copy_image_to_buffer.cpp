#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include "../image_format.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

VSL_NAMESPACE::command::CopyImageToBuffer::CopyImageToBuffer(VSL_NAMESPACE::BufferAccessor* buffer, VSL_NAMESPACE::ImageAccessor image)
    : CopyImageToBuffer(buffer, std::move(image), ImageLayout::GENERAL) {}

VSL_NAMESPACE::command::CopyImageToBuffer::CopyImageToBuffer(VSL_NAMESPACE::BufferAccessor* buffer, VSL_NAMESPACE::ImageAccessor image,
                                                             ImageLayout layout, ImageRegion region, bool host_read)
    : CopyImageToBuffer(buffer ? buffer->data_ : nullptr, std::move(image), layout, region, host_read) {
    this->buffer = buffer;
}

VSL_NAMESPACE::command::CopyImageToBuffer::CopyImageToBuffer(std::shared_ptr<impl::BufferImpl> buffer,
                                                             VSL_NAMESPACE::ImageAccessor image, ImageLayout layout,
                                                             ImageRegion region, bool host_read)
    : CopyImageToBuffer(buffer, std::move(image), layout, region, host_read, 0, buffer ? buffer->requirement_size : 0) {}

VSL_NAMESPACE::command::CopyImageToBuffer::CopyImageToBuffer(std::shared_ptr<impl::BufferImpl> buffer,
                                                             VSL_NAMESPACE::ImageAccessor image, ImageLayout layout,
                                                             ImageRegion region, bool host_read, std::size_t offset, std::size_t size)
    : image(std::move(image)), buffer(nullptr), layout(layout), region(region), host_read(host_read),
      _destination(std::move(buffer)), _destination_offset(offset), _destination_size(size) {}

VSL_NAMESPACE::command::ImageCopyInfo VSL_NAMESPACE::command::CopyImageToBuffer::Describe(ImageAccessor image, ImageRegion region) {
    if (!image.data_ || !image.data_->image || !image.data_->device)
        throw std::invalid_argument("Image copy requires a valid image");
    const auto& source = *image.data_;
    if (!contain(source.usage, ImageType::TRANSFER_SRC) || source.samples != SampleCount::ONE ||
        impl::helper::image_aspects(source.format) != vk::ImageAspectFlagBits::eColor)
        throw std::invalid_argument("Image copy requires a single-sample color TransferSrc image");
    auto area = region;
    if (!area.width && !area.height) {
        if (area.x || area.y)
            throw std::invalid_argument("Full-image copy requires origin zero");
        area.width = source.width;
        area.height = source.height;
    }
    if (!area.width || !area.height || area.x >= source.width || area.y >= source.height ||
        area.width > source.width - area.x || area.height > source.height - area.y || area.layer >= source.count)
        throw std::out_of_range("Image copy region exceeds image");
    const auto texel_size = data_format::convert(static_cast<data_format::FormatTypes>(source.format)).size();
    if (!texel_size)
        throw std::invalid_argument("Image copy format has no supported texel size");
    const std::uint64_t row = std::uint64_t(area.width) * texel_size;
    if (row > std::numeric_limits<std::size_t>::max() / area.height)
        throw std::length_error("Image copy byte count overflow");
    const auto bytes = row * area.height;
    return {area, image.get_format(), static_cast<std::size_t>(row), static_cast<std::size_t>(bytes)};
}

VSL_NAMESPACE::command::ImageCopyInfo VSL_NAMESPACE::command::CopyImageToBuffer::description() const {
    const auto info = Describe(image, region);
    const auto& destination = _destination;
    if (!destination || !destination->buffer || destination->device != image.data_->device.get())
        throw std::invalid_argument("Image copy requires a destination on the same device");
    if (!contain(destination->usage, MemoryType::TRANSFER_DESTINATION))
        throw std::invalid_argument("Image copy requires TransferDestination buffer usage");
    if (host_read && !contain(destination->properties, MemoryProperty::HOST_VISIBLE))
        throw std::invalid_argument("Host read requires host-visible buffer memory");
    if (layout != ImageLayout::GENERAL && layout != ImageLayout::TRANSFER_SRC_OPTIMAL)
        throw std::invalid_argument("Image copy requires General or TransferSrcOptimal layout");
    if (_destination_offset > destination->requirement_size ||
        _destination_size > destination->requirement_size - _destination_offset || info.byte_size > _destination_size)
        throw std::out_of_range("Image copy exceeds destination buffer");
    if (_destination_offset % info.format.size())
        throw std::invalid_argument("Image copy destination offset must be aligned to the texel size");
    return info;
}

void VSL_NAMESPACE::command::CopyImageToBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    const auto info = description();
    const auto& destination = _destination;
    if (!buffer.data_ || !buffer.data_->command_pool || !manager.data_ ||
        manager.data_->device != image.data_->device ||
        buffer.data_->command_pool->device != image.data_->device)
        throw std::invalid_argument("Image copy requires a command buffer on the same device");
    buffer.retain(image.data_);
    buffer.retain(destination);
    const auto& area = info.region;
    vk::BufferImageCopy copy{
        .bufferOffset = _destination_offset,
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, area.layer, 1},
        .imageOffset = {static_cast<std::int32_t>(area.x), static_cast<std::int32_t>(area.y), 0},
        .imageExtent = {area.width, area.height, 1}};
    auto command = buffer.data_->command_buffers.at(buffer.get_current_buffer_idx());
    command.copyImageToBuffer(image.data_->image, static_cast<vk::ImageLayout>(layout), destination->buffer, copy);
    if (host_read) {
        vk::BufferMemoryBarrier barrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eHostRead,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = destination->buffer,
            .offset = _destination_offset,
            .size = info.byte_size};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
                                {}, {}, barrier, {});
    }
}
