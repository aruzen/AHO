#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../_pimpls.h"

#include "bind_index_buffers.h"

void VSL_NAMESPACE::command::BindIndexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
		.bindIndexBuffer(this->buffer->_data->buffer, offset, vk::IndexType::eUint32);
}
