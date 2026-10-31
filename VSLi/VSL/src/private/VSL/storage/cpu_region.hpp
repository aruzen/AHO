#pragma once
#include <VSL/storage/arena_memory.hpp>

namespace VSL_NAMESPACE::storage::detail {
    struct CPURegion {
        std::shared_ptr<void> external_owner;
        std::byte* base = nullptr;
        size_t bytes, alignment;
        bool allocated = false, claimed = false;
        CPURegion(size_t bytes, size_t alignment);
        CPURegion(std::span<std::byte> region, size_t alignment, std::shared_ptr<void> owner);
        ~CPURegion();
        void claim(size_t required_bytes, size_t required_alignment);
        void release() noexcept { claimed = false; }
    };
} // namespace VSL_NAMESPACE::storage::detail
