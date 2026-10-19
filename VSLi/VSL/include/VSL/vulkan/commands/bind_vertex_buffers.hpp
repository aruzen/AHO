#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../buffer.hpp"
#include <concepts>

namespace VSL_NAMESPACE::command {
    struct BindVertexBuffer : __Command {
        template<typename... Args>
        requires((std::convertible_to<Args*, BufferAccessor*> &&
                  ((unsigned int)(Args::Type & MemoryType::VertexBuffer) != 0)) &&
                 ...)
        BindVertexBuffer(Args&... args);

        std::vector<BufferAccessor*> buffers;

        template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode = SharingMode::Exclusive>
        requires((unsigned int)(MemType & MemoryType::VertexBuffer) != 0)
        void add(Buffer<MemType, MemProperty, SharingMode>&);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    // ===============================================================================================

    template<typename... Args>
    requires((std::convertible_to<Args*, BufferAccessor*> &&
              ((unsigned int)(Args::Type & MemoryType::VertexBuffer) != 0)) &&
             ...)
    VSL_NAMESPACE::command::BindVertexBuffer::BindVertexBuffer(Args&... args) {
        (..., this->add(args));
    }

    template<VSL_NAMESPACE::MemoryType MemType, VSL_NAMESPACE::MemoryProperty MemProperty, SharingMode SharingMode>
    requires((unsigned int)(MemType & MemoryType::VertexBuffer) != 0)
    void VSL_NAMESPACE::command::BindVertexBuffer::add(VSL_NAMESPACE::Buffer<MemType, MemProperty, SharingMode>& buffer) {
        buffers.push_back(&buffer);
    }
} // namespace VSL_NAMESPACE::command
