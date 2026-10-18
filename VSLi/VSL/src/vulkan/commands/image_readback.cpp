#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <stdexcept>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::command {
    ImageReadback::ImageReadback(BufferAccessor *destination, ImageAccessor image, ImageLayout currentLayout,
                                 ImageRegion region)
        : copy(destination, image, ImageLayout::TransferSrcOptimal, region, true), currentLayout(currentLayout),
          destinationOwner_(destination ? destination->_data : nullptr) {}

    ImageReadback::ImageReadback(std::shared_ptr<_impl::Buffer_impl> destination, ImageAccessor image,
                                 ImageLayout currentLayout, ImageRegion region)
        : copy(destination, image, ImageLayout::TransferSrcOptimal, region, true), currentLayout(currentLayout),
          destinationOwner_(destination) {}

    ImageReadback::ImageReadback(MappedBuffer destination, ImageAccessor image,
                                 ImageLayout currentLayout, ImageRegion region)
        : copy(destination._data ? destination._data->buffer : nullptr, image,
               ImageLayout::TransferSrcOptimal, region, true),
          currentLayout(currentLayout),
          destinationOwner_(destination._data) {
        if (destination._data && !contain(destination._data->usage, MemoryType::TransferDestination))
            throw std::invalid_argument("Readback buffer requires TransferDestination usage");
    }

    ImageCopyInfo ImageReadback::describe(ImageAccessor image, ImageLayout layout, ImageRegion region) {
        switch (layout) {
            case ImageLayout::General:
            case ImageLayout::ColorAttachmentOptimal:
            case ImageLayout::ShaderReadOnlyOptimal:
            case ImageLayout::TransferSrcOptimal:
            case ImageLayout::TransferDstOptimal:
            case ImageLayout::Present:
                return CopyImageToBuffer::describe(image, region);
            default:
                throw std::invalid_argument("Readback requires defined color image contents");
        }
    }

    void ImageReadback::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        const auto info = describe(copy.image, currentLayout, copy.region);
        (void)copy.description(); // 遷移の記録前にコピー先を検証する。
        if (copy.layout != ImageLayout::TransferSrcOptimal || !copy.hostRead)
            throw std::invalid_argument("Readback copy must use TransferSrcOptimal and host visibility");
        if (auto submission = buffer._data->recordingSubmission) {
            submission->resources.reserve(submission->resources.size() + 2);
            submission->resources.push_back(destinationOwner_);
            submission->resources.push_back(copy.image._data);
        }
        ChangeImageBarrier(copy.image, currentLayout, ImageLayout::TransferSrcOptimal, info.region.layer, 1)
            .invoke(pool, buffer, manager);
        copy.invoke(pool, buffer, manager);
        ChangeImageBarrier(copy.image, ImageLayout::TransferSrcOptimal, currentLayout, info.region.layer, 1)
            .invoke(pool, buffer, manager);
    }
}
