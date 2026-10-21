#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <stdexcept>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::command {
    ImageReadback::ImageReadback(BufferAccessor* destination, ImageAccessor image, ImageLayout current_layout,
                                 ImageRegion region)
        : copy(destination, image, ImageLayout::TRANSFER_SRC_OPTIMAL, region, true), current_layout(current_layout) {}

    ImageReadback::ImageReadback(std::shared_ptr<impl::BufferImpl> destination, ImageAccessor image,
                                 ImageLayout current_layout, ImageRegion region)
        : copy(destination, image, ImageLayout::TRANSFER_SRC_OPTIMAL, region, true), current_layout(current_layout) {}

    ImageCopyInfo ImageReadback::Describe(ImageAccessor image, ImageLayout layout, ImageRegion region) {
        switch (layout) {
            case ImageLayout::GENERAL:
            case ImageLayout::COLOR_ATTACHMENT_OPTIMAL:
            case ImageLayout::SHADER_READ_ONLY_OPTIMAL:
            case ImageLayout::TRANSFER_SRC_OPTIMAL:
            case ImageLayout::TRANSFER_DST_OPTIMAL:
            case ImageLayout::PRESENT:
                return CopyImageToBuffer::Describe(image, region);
            default:
                throw std::invalid_argument("Readback requires defined color image contents");
        }
    }

    void ImageReadback::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        const auto info = Describe(copy.image, current_layout, copy.region);
        (void)copy.description(); // 遷移の記録前にコピー先を検証する。
        if (copy.layout != ImageLayout::TRANSFER_SRC_OPTIMAL || !copy.host_read)
            throw std::invalid_argument("Readback copy must use TransferSrcOptimal and host visibility");
        if (!buffer.data_ || !buffer.data_->command_pool || !manager.data_ ||
            manager.data_->device != copy.image.data_->device ||
            buffer.data_->command_pool->device != copy.image.data_->device)
            throw std::invalid_argument("Readback requires a command buffer and manager on the image device");
        if (auto submission = buffer.data_->recording_submission) {
            submission->resources.reserve(submission->resources.size() + 4);
        }
        ChangeImageBarrier(copy.image, current_layout, ImageLayout::TRANSFER_SRC_OPTIMAL, info.region.layer, 1)
            .invoke(pool, buffer, manager);
        copy.invoke(pool, buffer, manager);
        ChangeImageBarrier(copy.image, ImageLayout::TRANSFER_SRC_OPTIMAL, current_layout, info.region.layer, 1)
            .invoke(pool, buffer, manager);
    }
} // namespace VSL_NAMESPACE::command
