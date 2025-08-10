//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../_pimpls.h"

#include "pipeline_barrier.hpp"

vsl::command::PipelineBarrierChange::PipelineBarrierChange(vsl::Image image) : image(image) {}

void vsl::command::PipelineBarrierChange::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer,
                                                 vsl::CommandManager manager) {
    vk::ImageMemoryBarrier barrier;
    // TODO
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eGeneral;
    barrier.image = image._data->image;  // 対象の vk::Image
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = {};  // UNDEFINED なので何もなし
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eComputeShader,
            {},
            nullptr,
            nullptr,
            barrier
    );
}
