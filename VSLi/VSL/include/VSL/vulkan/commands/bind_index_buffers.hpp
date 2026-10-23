#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../buffer.hpp"

namespace VSL_NAMESPACE::command {
    enum class IndexType {
        UINT16,
        UINT32
    };
    struct BindIndexBufferAt : Command {
        BindIndexBufferAt(const BufferAccessor& source, IndexType type, size_t offset = 0);
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
        const std::shared_ptr<impl::BufferImpl>& source() const { return _source; }
        size_t offset() const { return _offset; }
        IndexType type() const { return _type; }

    private:
        std::shared_ptr<impl::BufferImpl> _source;
        size_t _offset;
        IndexType _type;
    };
    struct BindIndexBuffer : Command {
        template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode = SharingMode::EXCLUSIVE>
        requires((unsigned int)(mem_type & MemoryType::INDEX_BUFFER) != 0)
        BindIndexBuffer(Buffer<mem_type, mem_property, sharing_mode>&, uint32_t offset = 0);

        BufferAccessor* buffer = nullptr;
        size_t offset;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    // ===============================================================================================

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode>
    requires((unsigned int)(mem_type & MemoryType::INDEX_BUFFER) != 0)
    VSL_NAMESPACE::command::BindIndexBuffer::BindIndexBuffer(Buffer<mem_type, mem_property, sharing_mode>& buffer, uint32_t offset) : buffer(&buffer), offset(offset) {}
} // namespace VSL_NAMESPACE::command
