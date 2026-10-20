#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include "../image_format.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::command {
    CopyBufferToImage::CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer, ImageLayout layout)
        : CopyBufferToImage(std::move(image), buffer, layout, {}) {}
    CopyBufferToImage::CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer, ImageLayout layout, ImageRegion region, std::size_t offset)
        : layout(layout), image(std::move(image)), buffer(buffer), region(region), offset(offset), source_(buffer ? buffer->_data : nullptr) {}
    ImageCopyInfo CopyBufferToImage::description() const {
        if (!image._data || !image._data->image || !contain(image._data->usage, ImageType::TransferDst) ||
            image._data->samples != SampleCount::One || _impl::helper::imageAspects(image._data->format) != vk::ImageAspectFlagBits::eColor)
            throw std::invalid_argument("Image upload requires a single-sample color TransferDst image");
        if (layout != ImageLayout::General && layout != ImageLayout::TransferDstOptimal)
            throw std::invalid_argument("Image upload requires General or TransferDstOptimal layout");
        const auto& source = source_;
        if (!source || !source->buffer || source->device != image._data->device)
            throw std::invalid_argument("Image upload requires a source on the same device");
        if (!contain(source->usage, MemoryType::TransferSource))
            throw std::invalid_argument("Image upload requires TransferSource buffer usage");
        auto area = region;
        if (!area.width && !area.height) {
            if (area.x || area.y)
                throw std::invalid_argument("Full-image upload requires origin zero");
            area.width = image._data->width;
            area.height = image._data->height;
        }
        if (!area.width || !area.height || area.x >= image._data->width || area.y >= image._data->height ||
            area.width > image._data->width - area.x || area.height > image._data->height - area.y || area.layer >= image._data->count)
            throw std::out_of_range("Image upload region exceeds image");
        const auto texel = image.getFormat().size();
        if (!texel || offset % 4 || offset % texel)
            throw std::invalid_argument("Image upload source offset alignment");
        const std::uint64_t row = std::uint64_t(area.width) * texel;
        if (row > std::numeric_limits<std::size_t>::max() / area.height)
            throw std::length_error("Image upload byte count overflow");
        const auto bytes = static_cast<std::size_t>(row * area.height);
        if (offset > source->requirementSize || bytes > source->requirementSize - offset)
            throw std::out_of_range("Image upload exceeds source buffer");
        return {area, image.getFormat(), static_cast<std::size_t>(row), bytes};
    }
    void CopyBufferToImage::invoke(CommandPool, CommandBuffer commands, CommandManager manager) {
        const auto info = description();
        if (!commands._data || !commands._data->commandPool || !manager._data ||
            commands._data->commandPool->device != image._data->device || manager.getDevice() != image.getDevice())
            throw std::invalid_argument("Image upload command device mismatch");
        const auto& source = source_;
        commands.retain(source);
        commands.retain(image._data);
        const auto& area = info.region;
        vk::BufferImageCopy copy{.bufferOffset = offset,
                                 .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, area.layer, 1},
                                 .imageOffset = {static_cast<std::int32_t>(area.x), static_cast<std::int32_t>(area.y), 0},
                                 .imageExtent = {area.width, area.height, 1}};
        commands._data->commandBuffers.at(commands.getCurrentBufferIdx()).copyBufferToImage(source->buffer, image._data->image, static_cast<vk::ImageLayout>(layout), copy);
    }
} // namespace VSL_NAMESPACE::command

VSL_NAMESPACE::command::CopyBufferToArrayImage::CopyBufferToArrayImage(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::BufferAccessor* buffer,
                                                                       VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx)
    : image(std::move(image)), buffer(buffer), layout(layout), target_idx(target_idx), source_(buffer ? buffer->_data : nullptr) {}

void VSL_NAMESPACE::command::CopyBufferToArrayImage::invoke(
    VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    if (!source_ || !VSL_NAMESPACE::contain(source_->usage, VSL_NAMESPACE::MemoryType::TransferSource))
        throw std::invalid_argument("Image upload requires TransferSource buffer usage");
    buffer.retain(image._data);
    buffer.retain(source_);
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0; // tightly packed
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = target_idx;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{image._data->width, image._data->height, 1};

    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].copyBufferToImage(
        source_->buffer,
        this->image._data->image,
        (vk::ImageLayout)layout,
        region);
}
