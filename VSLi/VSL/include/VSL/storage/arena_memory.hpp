#pragma once
#include "../define.hpp"
#include <cstddef>
#include <memory>
#include <span>

namespace VSL_NAMESPACE::storage {
    class BufferArena;
    namespace detail {
        struct CPURegion;
    }
    // CPU領域だけを管理する。コピーは同じ領域と使用権を共有する。
    class ArenaMemory {
        std::shared_ptr<detail::CPURegion> _region;
        friend class BufferArena;

    public:
        ArenaMemory(size_t bytes, size_t alignment);
        // 外部領域を一つの管理単位としてadoptする。重複したadoptは行わない。
        ArenaMemory(std::span<std::byte> region, size_t alignment, std::shared_ptr<void> owner);
        size_t size() const;
        size_t alignment() const;
    };
} // namespace VSL_NAMESPACE::storage
