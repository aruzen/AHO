#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/operation/image_barrier.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <stdexcept>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE::command {
    ImageReadback::ImageReadback(BufferAccessor* destination, ImageAccessor image, ImageLayout current_layout,
                                 ImageRegion region)
        : copy(destination, image, ImageLayout::TRANSFER_SRC_OPTIMAL, region, true), current_layout(current_layout) {}

    ImageReadback::ImageReadback(BufferView destination, ImageAccessor image, ImageLayout current_layout,
                                 ImageRegion region, ResourceUse constraint, ResourceRetention retention)
        : copy(std::move(destination), std::move(image), ImageLayout::TRANSFER_SRC_OPTIMAL, region, true, std::move(constraint), retention), current_layout(current_layout) {}

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
        const auto request = *this;
        const auto& copy = request.copy;
        const auto current_layout = request.current_layout;
        const auto info = Describe(copy.image, current_layout, copy.region);
        (void)copy.description(); // 遷移の記録前にコピー先を検証する。
        if (copy.layout != ImageLayout::TRANSFER_SRC_OPTIMAL || !copy.host_read)
            throw std::invalid_argument("Readback copy must use TransferSrcOptimal and host visibility");
        if (!CommandAccess::Data(buffer) || !CommandAccess::Data(buffer)->command_pool || !CommandAccess::Data(manager) ||
            CommandAccess::Data(manager)->device.get() != ImageAccess::Owned(copy.image)->device ||
            CommandAccess::Data(buffer)->command_pool->device != ImageAccess::Owned(copy.image)->device)
            throw std::invalid_argument("Readback requires a command buffer and manager on the image device");
        const auto before = operation::prepare_image_barrier(*ImageAccess::Owned(copy.image), current_layout, ImageLayout::TRANSFER_SRC_OPTIMAL, info.region.layer, 1);
        const auto after = operation::prepare_image_barrier(*ImageAccess::Owned(copy.image), ImageLayout::TRANSFER_SRC_OPTIMAL, current_layout, info.region.layer, 1);
        const auto prepared = copy.prepare(buffer, manager);
        const NativeRecordingContext recording(buffer);
        copy.track(recording);
        operation::record_image_barrier(vk::CommandBuffer(recording.native_command_buffer()), before);
        copy.record(prepared, recording);
        operation::record_image_barrier(vk::CommandBuffer(recording.native_command_buffer()), after);
    }
} // namespace VSL_NAMESPACE::command
