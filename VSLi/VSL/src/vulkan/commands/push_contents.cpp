//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/vulkan/commands/push_contents.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void vsl::command::PushConstant::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
            .pushConstants(layout._data->pipelineLayout, (vk::ShaderStageFlagBits) targetType, offset, size, value);
}