//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/vulkan/commands/push_content.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::command::PushConstant::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer.retain(layout.data_);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()]
        .pushConstants(layout.data_->pipeline_layout, (vk::ShaderStageFlagBits)target_type, offset, size, value);
}
