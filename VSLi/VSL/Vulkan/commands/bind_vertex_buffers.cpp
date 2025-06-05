#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../_pimpls.h"

#include "bind_vertex_buffers.h"

#include <ranges>

void VSL_NAMESPACE::command::BindVertexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
		.bindVertexBuffers(0, this->buffers
        | std::views::transform([](auto& b){ return b->_data->buffer; })
        | std::ranges::to<std::vector<vk::Buffer>>(),
        std::vector<vk::DeviceSize>(this->buffers.size(), 0));
}

