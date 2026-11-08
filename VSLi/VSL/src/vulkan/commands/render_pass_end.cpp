#include <VSL/lifetime/command_domain.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/frame_buffer_ops.hpp>
#include <stdexcept>

void VSL_NAMESPACE::command::RenderPassEnd::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
    if (buffer.data_->recording_context->frame_buffer_recording)
        throw std::logic_error("Render pass end cannot reenter framebuffer tracking");
    operation::record_frame_buffer_end(buffer.data_->command_buffers.at(buffer.get_current_buffer_idx()));
}
