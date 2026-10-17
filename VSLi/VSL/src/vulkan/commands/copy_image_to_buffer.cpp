//
// Created by morimoto_hibiki on 2025/10/22.
//

#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include "../image_format.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

vsl::command::CopyImageToBuffer::CopyImageToBuffer(vsl::BufferAccessor *buffer, vsl::ImageAccessor image)
    : CopyImageToBuffer(buffer, std::move(image), ImageLayout::General) {}

vsl::command::CopyImageToBuffer::CopyImageToBuffer(vsl::BufferAccessor *buffer, vsl::ImageAccessor image,
                                                   ImageLayout layout, ImageRegion region, bool hostRead)
    : image(std::move(image)), buffer(buffer), layout(layout), region(region), hostRead(hostRead) {}

vsl::command::CopyImageToBuffer::CopyImageToBuffer(std::shared_ptr<_impl::Buffer_impl> buffer,
                                                   vsl::ImageAccessor image, ImageLayout layout,
                                                   ImageRegion region, bool hostRead)
    : image(std::move(image)), buffer(nullptr), layout(layout), region(region), hostRead(hostRead),
      destination_(std::move(buffer)) {}

vsl::command::ImageCopyInfo vsl::command::CopyImageToBuffer::describe(ImageAccessor image, ImageRegion region) {
    if (!image._data || !image._data->image || !image._data->device)
        throw std::invalid_argument("Image copy requires a valid image");
    const auto &source = *image._data;
    if (!contain(source.usage, ImageType::TransferSrc) || source.samples != SampleCount::One ||
        _impl::helper::imageAspects(source.format) != vk::ImageAspectFlagBits::eColor)
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
    const auto texelSize = data_format::convert(static_cast<data_format::___FormatTypes>(source.format)).size();
    if (!texelSize)
        throw std::invalid_argument("Image copy format has no supported texel size");
    const std::uint64_t row = std::uint64_t(area.width) * texelSize;
    if (row > std::numeric_limits<std::size_t>::max() / area.height)
        throw std::length_error("Image copy byte count overflow");
    const auto bytes = row * area.height;
    return {area, image.getFormat(), static_cast<std::size_t>(row), static_cast<std::size_t>(bytes)};
}

vsl::command::ImageCopyInfo vsl::command::CopyImageToBuffer::description() const {
    const auto info = describe(image, region);
    const auto destination = buffer ? buffer->_data : destination_;
    if (!destination || !destination->buffer || destination->device != image._data->device)
        throw std::invalid_argument("Image copy requires a destination on the same device");
    if (layout != ImageLayout::General && layout != ImageLayout::TransferSrcOptimal)
        throw std::invalid_argument("Image copy requires General or TransferSrcOptimal layout");
    if (info.byteSize > destination->requirementSize)
        throw std::out_of_range("Image copy exceeds destination buffer");
    return info;
}

void vsl::command::CopyImageToBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    const auto info = description();
    const auto destination = this->buffer ? this->buffer->_data : destination_;
    if (!buffer._data || !buffer._data->commandPool || !manager._data ||
        manager._data->device != image._data->device ||
        buffer._data->commandPool->device != image._data->device)
        throw std::invalid_argument("Image copy requires a command buffer on the same device");
    const auto &area = info.region;
    vk::BufferImageCopy copy{
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, area.layer, 1},
        .imageOffset = {static_cast<std::int32_t>(area.x), static_cast<std::int32_t>(area.y), 0},
        .imageExtent = {area.width, area.height, 1}};
    auto command = buffer._data->commandBuffers.at(buffer.getCurrentBufferIdx());
    command.copyImageToBuffer(image._data->image, static_cast<vk::ImageLayout>(layout), destination->buffer, copy);
    if (hostRead) {
        vk::BufferMemoryBarrier barrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eHostRead,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = destination->buffer,
            .offset = 0,
            .size = info.byteSize};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
                                {}, {}, barrier, {});
    }
}
