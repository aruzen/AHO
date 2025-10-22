//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../_pimpls.h"

#include "change_image_barrier.hpp"

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
    barrier.subresourceRange.layerCount = 1;

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
