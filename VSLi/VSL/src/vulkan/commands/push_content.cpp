//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/vulkan/commands/push_content.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::command::PushConstant::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
        .pushConstants(layout._data->pipelineLayout, (vk::ShaderStageFlagBits)targetType, offset, size, value);
}
