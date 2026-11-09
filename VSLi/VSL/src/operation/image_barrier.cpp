#include <VSL/operation/image_barrier.hpp>
#include "../vulkan/image_format.hpp"
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    ImageBarrier prepare_image_barrier(const impl::ImageImpl& image, ImageLayout old_layout, ImageLayout new_layout,
                                       std::uint32_t base_layer, std::uint32_t layer_count, std::uint32_t shader_stages) {
        if (!image.image || !image.device)
            throw std::invalid_argument("Image barrier requires a live image");
        if (base_layer >= image.count)
            throw std::out_of_range("Image barrier base layer exceeds image");
        const auto layers = layer_count ? layer_count : image.count - base_layer;
        if (layers > image.count - base_layer)
            throw std::out_of_range("Image barrier layer range exceeds image");
        vk::ImageMemoryBarrier barrier;
        barrier.oldLayout = (vk::ImageLayout)old_layout;
        barrier.newLayout = (vk::ImageLayout)new_layout;
        barrier.image = image.image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = impl::helper::image_aspects(image.format);
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = base_layer;
        barrier.subresourceRange.layerCount = layers;

        vk::PipelineStageFlags source_stage, destination_stage;
        if (old_layout == ImageLayout::UNDEFINED && new_layout == ImageLayout::TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && new_layout == ImageLayout::TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            source_stage = vk::PipelineStageFlagBits::eAllCommands;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == ImageLayout::UNDEFINED && new_layout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        } else if (old_layout == ImageLayout::TRANSFER_DST_OPTIMAL && new_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlags(shader_stages);
        } else if (old_layout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && new_layout == ImageLayout::TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

            source_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == ImageLayout::TRANSFER_SRC_OPTIMAL && new_layout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        } else if (new_layout == ImageLayout::TRANSFER_SRC_OPTIMAL &&
                   (old_layout == ImageLayout::GENERAL || old_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL ||
                    old_layout == ImageLayout::TRANSFER_DST_OPTIMAL || old_layout == ImageLayout::PRESENT ||
                    old_layout == ImageLayout::TRANSFER_SRC_OPTIMAL)) {
            barrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            source_stage = vk::PipelineStageFlagBits::eAllCommands;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == ImageLayout::TRANSFER_SRC_OPTIMAL &&
                   (new_layout == ImageLayout::GENERAL || new_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL ||
                    new_layout == ImageLayout::TRANSFER_DST_OPTIMAL || new_layout == ImageLayout::PRESENT)) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
            barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eAllCommands;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        return {barrier, source_stage, destination_stage};
    }
    void record_image_barrier(vk::CommandBuffer commands, const ImageBarrier& barrier) {
        commands.pipelineBarrier(barrier.source, barrier.destination, {}, nullptr, nullptr, barrier.barrier);
    }
} // namespace VSL_NAMESPACE::operation
