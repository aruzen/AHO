#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <stdexcept>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
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
        if (!CommandAccess::Data(buffer) || !CommandAccess::Data(buffer)->command_pool || !CommandAccess::Data(manager) ||
            CommandAccess::Data(manager)->device.get() != copy.image.data_->device ||
            CommandAccess::Data(buffer)->command_pool->device != copy.image.data_->device)
            throw std::invalid_argument("Readback requires a command buffer and manager on the image device");
        if (auto submission = CommandAccess::Data(buffer)->recording_context->work) {
            submission->resources.reserve(submission->resources.size() + 4);
        }
        ChangeImageBarrier(copy.image, current_layout, ImageLayout::TRANSFER_SRC_OPTIMAL, info.region.layer, 1)
            .invoke(pool, buffer, manager);
        copy.invoke(pool, buffer, manager);
        ChangeImageBarrier(copy.image, ImageLayout::TRANSFER_SRC_OPTIMAL, current_layout, info.region.layer, 1)
            .invoke(pool, buffer, manager);
    }
} // namespace VSL_NAMESPACE::command
