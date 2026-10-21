#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::command {
    ImageUpload::ImageUpload(BufferAccessor* source, ImageAccessor image, ImageLayout layout, ImageRegion region, std::size_t offset)
        : copy(image, source, ImageLayout::TransferDstOptimal, region, offset), currentLayout(layout) {
    }

    void ImageUpload::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        (void)copy.description();
        if (currentLayout != ImageLayout::Undefined && currentLayout != ImageLayout::ShaderReadOnlyOptimal)
            throw std::invalid_argument("ImageUpload requires Undefined or ShaderReadOnlyOptimal layout");
        if (!buffer._data || !buffer._data->commandPool || !manager._data ||
            buffer._data->commandPool->device != copy.image._data->device || manager.getDevice() != copy.image.getDevice())
            throw std::invalid_argument("ImageUpload command device mismatch");
        if (copy.layout != ImageLayout::TransferDstOptimal)
            throw std::invalid_argument("ImageUpload copy layout was modified");
        if (!contain(copy.image._data->usage, ImageType::Sampled))
            throw std::invalid_argument("ImageUpload requires a sampled image");
        if (auto submission = buffer._data->recordingSubmission)
            submission->resources.reserve(submission->resources.size() + 8);
        buffer.retain(copy.image._data);
        ChangeImageBarrier(copy.image, currentLayout, ImageLayout::TransferDstOptimal).invoke(pool, buffer, manager);
        if (currentLayout == ImageLayout::Undefined) {
            const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, copy.image._data->count};

            auto command = buffer._data->commandBuffers.at(buffer.getCurrentBufferIdx());
            command.clearColorImage(copy.image._data->image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{}, range);
            const vk::MemoryBarrier visible{.srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eTransferWrite};

            command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, visible, {}, {});
        }

        copy.invoke(pool, buffer, manager);
        ChangeImageBarrier(copy.image, ImageLayout::TransferDstOptimal, ImageLayout::ShaderReadOnlyOptimal).invoke(pool, buffer, manager);
    }
} // namespace VSL_NAMESPACE::command
