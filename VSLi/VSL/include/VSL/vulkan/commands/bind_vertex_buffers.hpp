#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../buffer.hpp"
#include <concepts>

namespace VSL_NAMESPACE::command {
    struct BindVertexBuffer : Command {
        template<typename... Args>
        requires((std::convertible_to<Args*, BufferAccessor*> &&
                  ((unsigned int)(Args::TYPE & MemoryType::VERTEX_BUFFER) != 0)) &&
                 ...)
        BindVertexBuffer(Args&... args);

        std::vector<BufferAccessor*> buffers;

        template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode = SharingMode::EXCLUSIVE>
        requires((unsigned int)(mem_type & MemoryType::VERTEX_BUFFER) != 0)
        void add(Buffer<mem_type, mem_property, sharing_mode>&);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    // ===============================================================================================

    template<typename... Args>
    requires((std::convertible_to<Args*, BufferAccessor*> &&
              ((unsigned int)(Args::TYPE & MemoryType::VERTEX_BUFFER) != 0)) &&
             ...)
    VSL_NAMESPACE::command::BindVertexBuffer::BindVertexBuffer(Args&... args) {
        (..., this->add(args));
    }

    template<VSL_NAMESPACE::MemoryType mem_type, VSL_NAMESPACE::MemoryProperty mem_property, SharingMode sharing_mode>
    requires((unsigned int)(mem_type & MemoryType::VERTEX_BUFFER) != 0)
    void VSL_NAMESPACE::command::BindVertexBuffer::add(VSL_NAMESPACE::Buffer<mem_type, mem_property, sharing_mode>& buffer) {
        buffers.push_back(&buffer);
    }
} // namespace VSL_NAMESPACE::command
