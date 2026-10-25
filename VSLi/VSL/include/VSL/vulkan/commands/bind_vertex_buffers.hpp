#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../buffer.hpp"
#include <concepts>

namespace VSL_NAMESPACE::command {
    // Explicit binding, with ownership retained even if the C++ buffer wrapper is destroyed.
    struct BindVertexBufferAt : Command {
        BindVertexBufferAt(std::uint32_t binding, const BufferAccessor& source, size_t offset = 0);
        BindVertexBufferAt(std::uint32_t binding, std::shared_ptr<impl::BufferImpl> source, size_t offset = 0);
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
        std::uint32_t binding() const { return _binding; }
        size_t offset() const { return _offset; }
        const std::shared_ptr<impl::BufferImpl>& source() const { return _source; }

    private:
        std::uint32_t _binding;
        size_t _offset;
        std::shared_ptr<impl::BufferImpl> _source;
    };

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
