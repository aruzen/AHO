#pragma once
#include "buffer_layout.hpp"
#include "arena_memory.hpp"
#include "../vulkan/buffer_view.hpp"

namespace VSL_NAMESPACE::storage {
    class BufferArena {
        std::shared_ptr<void> _owner;
        std::byte* _base = nullptr;
        size_t _count = 0;

    public:
        explicit BufferArena(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers)
            : BufferArena(device, buffers, ArenaMemory(BufferLayout::Bytes(buffers.size()), buffers.empty() ? 1 : BufferLayout::ALIGNMENT)) {}
        BufferArena(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers, ArenaMemory memory)
            : BufferArena((detail::require_buffer_layout(std::integral_constant<std::uint64_t, BUFFER_LAYOUT_ABI>{}), device), buffers, memory._region) {}
        size_t size() const { return _count; }
        const std::shared_ptr<void>& owner() const { return _owner; }
        // 借用値はarenaの有効scope内だけ利用する。rvalueからは取得しない。
        BufferRef borrow(size_t slot) const&;
        BufferRef borrow(size_t slot) const&& = delete;
        BufferView view(size_t slot) const;

    private:
        BufferArena(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers, std::shared_ptr<detail::CPURegion> memory);
    };
} // namespace VSL_NAMESPACE::storage
