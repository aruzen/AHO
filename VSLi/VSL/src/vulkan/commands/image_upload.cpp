#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/operation/image_barrier.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE::command {
    ImageUpload::ImageUpload(BufferAccessor* source, ImageAccessor image, ImageLayout layout, ImageRegion region, std::size_t offset)
        : copy(image, source, ImageLayout::TRANSFER_DST_OPTIMAL, region, offset), current_layout(layout) {
    }

    ImageUpload::ImageUpload(BufferView source, ImageAccessor image, ImageLayout layout, ImageRegion region, std::size_t offset,
                             ResourceUse constraint, ResourceRetention retention)
        : copy(std::move(image), std::move(source), ImageLayout::TRANSFER_DST_OPTIMAL, region, offset, std::move(constraint), retention), current_layout(layout) {}

    void ImageUpload::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        const auto upload = *this;
        const auto& copy = upload.copy;
        const auto current_layout = upload.current_layout;
        const auto info = copy.prepare(buffer, manager);
        if (current_layout != ImageLayout::UNDEFINED && current_layout != ImageLayout::SHADER_READ_ONLY_OPTIMAL)
            throw std::invalid_argument("ImageUpload requires UNDEFINED or SHADER_READ_ONLY_OPTIMAL layout");
        if (!CommandAccess::Data(buffer) || !CommandAccess::Data(buffer)->command_pool || !CommandAccess::Data(manager) ||
            CommandAccess::Data(buffer)->command_pool->device != ImageAccess::Owned(copy.image)->device || manager.get_device() != copy.image.get_device())
            throw std::invalid_argument("ImageUpload command device mismatch");
        if (copy.layout != ImageLayout::TRANSFER_DST_OPTIMAL)
            throw std::invalid_argument("ImageUpload copy layout was modified");
        if (!contain(ImageAccess::Owned(copy.image)->usage, ImageType::SAMPLED))
            throw std::invalid_argument("ImageUpload requires a sampled image");
        const auto before = operation::prepare_image_barrier(*ImageAccess::Owned(copy.image), current_layout, ImageLayout::TRANSFER_DST_OPTIMAL);
        const auto after = operation::prepare_image_barrier(*ImageAccess::Owned(copy.image), ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        const NativeRecordingContext recording(buffer);
        copy.track(recording);
        operation::record_image_barrier(vk::CommandBuffer(recording.native_command_buffer()), before);
        if (current_layout == ImageLayout::UNDEFINED) {
            const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, ImageAccess::Owned(copy.image)->count};

            const vk::CommandBuffer command(recording.native_command_buffer());
            command.clearColorImage(ImageAccess::Owned(copy.image)->image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{}, range);
            const vk::MemoryBarrier visible{.srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eTransferWrite};

            command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, visible, {}, {});
        }

        copy.record(info, recording);
        operation::record_image_barrier(vk::CommandBuffer(recording.native_command_buffer()), after);
    }
} // namespace VSL_NAMESPACE::command
