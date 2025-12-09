#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../buffer.hpp"

namespace VSL_NAMESPACE::command {
	struct BindIndexBuffer : __Command {
		template <MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode = SharingMode::Exclusive>
			requires ((unsigned int)(MemType & MemoryType::IndexBuffer) != 0)
		BindIndexBuffer(Buffer<MemType, MemProperty, SharingMode>, uint32_t offset = 0);

		BufferAccessor* buffer = nullptr;
        size_t offset;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

	// ===============================================================================================

	template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
		requires ((unsigned int)(MemType& MemoryType::IndexBuffer) != 0)
	VSL_NAMESPACE::command::BindIndexBuffer::BindIndexBuffer(Buffer<MemType, MemProperty, SharingMode> buffer, uint32_t offset) : buffer(&buffer), offset(offset) {}
}