#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../_pimpls.h"

#include "bind_vertex_buffers.h"

void VSL_NAMESPACE::command::BindVertexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
		.bindVertexBuffers(0, { this->buffer->_data->buffer }, { 0 });
}
