#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../buffer.hpp"

namespace VSL_NAMESPACE::command {
	struct BindVertexBuffer : __Command {
		template <typename ...Args>
        BindVertexBuffer(Args&... args);

		std::vector<BufferAccessor*> buffers;

        template <MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode = SharingMode::Exclusive>
            requires ((unsigned int)(MemType & MemoryType::VertexBuffer) != 0)
        void add(Buffer<MemType, MemProperty, SharingMode>&);

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

	// ===============================================================================================

    template<typename... Args>
    vsl::command::BindVertexBuffer::BindVertexBuffer(Args&... args) {
        (..., this->add(args));
    }

    template<vsl::MemoryType MemType, vsl::MemoryProperty MemProperty, SharingMode SharingMode>
        requires ((unsigned int) (MemType & MemoryType::VertexBuffer) != 0)
    void vsl::command::BindVertexBuffer::add(vsl::Buffer<MemType, MemProperty, SharingMode>& buffer) {
        buffers.push_back(&buffer);
    }
}