#pragma once
#include "../define.hpp"
#include "buffer_abi.hpp"
#include "../vulkan/resource_use.hpp"
#ifndef VSL_BUFFER_LAYOUT_HEADER
#error "Buffer placement requires a matching VSL CMake target and generated layout"
#endif
#include VSL_BUFFER_LAYOUT_HEADER
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace VSL_NAMESPACE::storage {
    static_assert(BUFFER_CPP_ABI_MODE == BUFFER_EXPECTED_CPP_ABI_MODE, "Buffer placement C++ ABI mode mismatch");
    static_assert(sizeof(SharingMode) == sizeof(std::uint32_t), "Buffer enum ABI mismatch");
    static_assert(sizeof(ResourceUseState) == BUFFER_USE_SIZE, "Buffer placement standard library ABI mismatch");
    static_assert(alignof(ResourceUseState) == BUFFER_USE_ALIGNMENT, "Buffer placement alignment mismatch");
    constexpr size_t checked_align(size_t bytes, size_t alignment);
    struct BufferLayout {
        static constexpr size_t USE_OFFSET = (BUFFER_IMPL_SIZE + BUFFER_USE_ALIGNMENT - 1) / BUFFER_USE_ALIGNMENT * BUFFER_USE_ALIGNMENT;
        static constexpr size_t ALIGNMENT = BUFFER_IMPL_ALIGNMENT > BUFFER_USE_ALIGNMENT ? BUFFER_IMPL_ALIGNMENT : BUFFER_USE_ALIGNMENT;
        static constexpr size_t STRIDE = (USE_OFFSET + BUFFER_USE_SIZE + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
        static constexpr size_t Offset(size_t slot);
        static constexpr size_t Bytes(size_t count) { return Offset(count); }
    };
    namespace detail {
        // ABI固有のsymbol。異なる生成情報のconsumerはlink時に拒否する。
        void require_buffer_layout(std::integral_constant<std::uint64_t, BUFFER_LAYOUT_ABI>);
    } // namespace detail
    constexpr size_t checked_align(size_t bytes, size_t alignment) {
        if (!alignment || (alignment & (alignment - 1)))
            throw std::invalid_argument("CPU storage alignment must be a power of two");
        if (bytes > std::numeric_limits<size_t>::max() - (alignment - 1))
            throw std::overflow_error("CPU storage alignment overflow");
        return (bytes + alignment - 1) & ~(alignment - 1);
    }
    constexpr size_t BufferLayout::Offset(size_t slot) {
        if (slot > std::numeric_limits<size_t>::max() / STRIDE)
            throw std::overflow_error("Buffer slot layout overflow");
        return slot * STRIDE;
    }
} // namespace VSL_NAMESPACE::storage
