#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include "../buffer.h"

namespace VSL_NAMESPACE::command {
	struct BindVertexBuffer : __Command {
		template <MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode = SharingMode::Exclusive>
			requires ((unsigned int)(MemType & MemoryType::VertexBuffer) != 0)
		BindVertexBuffer(Buffer<MemType, MemProperty, SharingMode>);

		BufferAccessor buffer;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

	// ===============================================================================================

	template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
		requires ((unsigned int)(MemType& MemoryType::VertexBuffer) != 0)
	VSL_NAMESPACE::command::BindVertexBuffer::BindVertexBuffer(Buffer<MemType, MemProperty, SharingMode> buffer) : buffer(buffer) {}
}