#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/resource_use_factory.hpp>
#include <VSL/vulkan/resource_use.hpp>

#include "../image_format.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
} // namespace

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
VSL_NAMESPACE::command::CopyImageToBuffer::CopyImageToBuffer(VSL_NAMESPACE::BufferAccessor* buffer, VSL_NAMESPACE::ImageAccessor image)
    : CopyImageToBuffer(buffer, std::move(image), ImageLayout::GENERAL) {}

VSL_NAMESPACE::command::CopyImageToBuffer::CopyImageToBuffer(VSL_NAMESPACE::BufferAccessor* buffer, VSL_NAMESPACE::ImageAccessor image,
                                                             ImageLayout layout, ImageRegion region, bool host_read)
    : image(std::move(image)), buffer(buffer), layout(layout), region(region), host_read(host_read),
      _destination(buffer ? BufferAccess::Owned(*buffer).get() : nullptr), _use(buffer ? buffer->resource_use() : ResourceUse{}),
      _destination_size(buffer ? buffer->requested_size() : 0) {}

VSL_NAMESPACE::command::CopyImageToBuffer::CopyImageToBuffer(BufferView destination, ImageAccessor image, ImageLayout layout,
                                                             ImageRegion region, bool host_read, ResourceUse constraint, ResourceRetention retention)
    : image(std::move(image)), buffer(nullptr), layout(layout), region(region), host_read(host_read),
      _destination(lifetime::detail::BufferAccess::Impl(destination.ref())), _use(destination.resource_use(std::move(constraint))),
      _destination_offset(destination.byte_offset()), _destination_size(destination.size()), _retention(retention) {}

VSL_NAMESPACE::command::ImageCopyInfo VSL_NAMESPACE::command::CopyImageToBuffer::Describe(ImageAccessor image, ImageRegion region) {
    if (!ImageAccess::Owned(image) || !ImageAccess::Owned(image)->image || !ImageAccess::Owned(image)->device)
        throw std::invalid_argument("Image copy requires a valid image");
    const auto& source = *ImageAccess::Owned(image);
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
    if (!destination || !destination->buffer || destination->device != ImageAccess::Owned(image)->device)
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

VSL_NAMESPACE::command::ImageCopyInfo VSL_NAMESPACE::command::CopyImageToBuffer::prepare(const CommandBuffer& buffer, const CommandManager& manager) const {
    const auto info = description();
    if (!CommandAccess::Data(buffer) || !CommandAccess::Data(buffer)->command_pool || !CommandAccess::Data(manager) ||
        CommandAccess::Data(manager)->device.get() != ImageAccess::Owned(image)->device ||
        CommandAccess::Data(buffer)->command_pool->device != ImageAccess::Owned(image)->device)
        throw std::invalid_argument("Image copy requires a command buffer on the same device");
    return info;
}

void VSL_NAMESPACE::command::CopyImageToBuffer::track(const NativeRecordingContext& recording) const {
    recording.use(image.resource_use(), ResourceRetention::SUBMISSION);
    recording.use(_use, _retention);
}

void VSL_NAMESPACE::command::CopyImageToBuffer::record(const ImageCopyInfo& info, const NativeRecordingContext& recording) const {
    const auto* destination = _destination;
    const auto& area = info.region;
    vk::BufferImageCopy copy{
        .bufferOffset = _destination_offset,
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, area.layer, 1},
        .imageOffset = {static_cast<std::int32_t>(area.x), static_cast<std::int32_t>(area.y), 0},
        .imageExtent = {area.width, area.height, 1}};
    const vk::CommandBuffer command(recording.native_command_buffer());
    command.copyImageToBuffer(ImageAccess::Owned(image)->image, static_cast<vk::ImageLayout>(layout), destination->buffer, copy);
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

void VSL_NAMESPACE::command::CopyImageToBuffer::invoke(CommandPool, CommandBuffer buffer, CommandManager manager) {
    const auto copy = *this;
    const auto info = copy.prepare(buffer, manager);
    const NativeRecordingContext recording(buffer);
    copy.track(recording);
    copy.record(info, recording);
}
