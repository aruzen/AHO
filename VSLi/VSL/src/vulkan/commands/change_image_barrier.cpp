//
// Created by morimoto_hibiki on 2025/07/30.
//
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/change_image_barrier.hpp>

vsl::command::ChangeImageBarrier::ChangeImageBarrier(vsl::ImageAccessor image,
                                                     ImageLayout oldLayout,
                                                     ImageLayout newLayout)
        : image(image), oldLayout(oldLayout), newLayout(newLayout) {}

void vsl::command::ChangeImageBarrier::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer,
                                                 vsl::CommandManager manager) {
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = (vk::ImageLayout)oldLayout;
    barrier.newLayout = (vk::ImageLayout)newLayout;
    barrier.image = image._data->image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = image._data->count;

    vk::PipelineStageFlags sourceStage, destinationStage;
    if (oldLayout == ImageLayout::Undefined && newLayout == ImageLayout::TransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == ImageLayout::TransferDstOptimal && newLayout == ImageLayout::ShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == ImageLayout::ColorAttachmentOptimal && newLayout == ImageLayout::TransferSrcOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == ImageLayout::TransferSrcOptimal && newLayout == ImageLayout::ColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
            sourceStage,
            destinationStage,
            {},
            nullptr,
            nullptr,
            barrier
    );
}
