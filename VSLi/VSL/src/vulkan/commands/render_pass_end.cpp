#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::command::RenderPassEnd::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].endRenderPass();
}
