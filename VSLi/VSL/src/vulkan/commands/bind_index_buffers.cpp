#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_index_buffers.hpp>

void VSL_NAMESPACE::command::BindIndexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.retain(this->buffer->data_);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()]
        .bindIndexBuffer(this->buffer->data_->buffer, offset, vk::IndexType::eUint32);
}
