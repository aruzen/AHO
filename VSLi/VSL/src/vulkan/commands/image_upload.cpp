#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::command {
    ImageUpload::ImageUpload(BufferAccessor* source, ImageAccessor image, ImageLayout layout, ImageRegion region, std::size_t offset)
        : copy(image, source, ImageLayout::TRANSFER_DST_OPTIMAL, region, offset), current_layout(layout) {
    }

    void ImageUpload::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        (void)copy.description();
        if (current_layout != ImageLayout::UNDEFINED && current_layout != ImageLayout::SHADER_READ_ONLY_OPTIMAL)
            throw std::invalid_argument("ImageUpload requires UNDEFINED or SHADER_READ_ONLY_OPTIMAL layout");
        if (!buffer.data_ || !buffer.data_->command_pool || !manager.data_ ||
            buffer.data_->command_pool->device != copy.image.data_->device || manager.get_device() != copy.image.get_device())
            throw std::invalid_argument("ImageUpload command device mismatch");
        if (copy.layout != ImageLayout::TRANSFER_DST_OPTIMAL)
            throw std::invalid_argument("ImageUpload copy layout was modified");
        if (!contain(copy.image.data_->usage, ImageType::SAMPLED))
            throw std::invalid_argument("ImageUpload requires a sampled image");
        if (auto submission = buffer.data_->recording_submission)
            submission->resources.reserve(submission->resources.size() + 8);
        buffer.retain(copy.image.data_);
        ChangeImageBarrier(copy.image, current_layout, ImageLayout::TRANSFER_DST_OPTIMAL).invoke(pool, buffer, manager);
        if (current_layout == ImageLayout::UNDEFINED) {
            const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, copy.image.data_->count};

            auto command = buffer.data_->command_buffers.at(buffer.get_current_buffer_idx());
            command.clearColorImage(copy.image.data_->image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{}, range);
            const vk::MemoryBarrier visible{.srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eTransferWrite};

            command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, visible, {}, {});
        }

        copy.invoke(pool, buffer, manager);
        ChangeImageBarrier(copy.image, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL).invoke(pool, buffer, manager);
    }
} // namespace VSL_NAMESPACE::command
