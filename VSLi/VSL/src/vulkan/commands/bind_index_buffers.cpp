#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_index_buffers.hpp>

void VSL_NAMESPACE::command::BindIndexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
		.bindIndexBuffer(this->buffer->_data->buffer, offset, vk::IndexType::eUint32);
}
