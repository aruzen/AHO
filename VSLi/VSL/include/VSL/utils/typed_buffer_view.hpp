#pragma once
#include "../define.hpp"
#include "../vulkan/buffer.hpp"
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace VSL_NAMESPACE::typed::detail {
    void validate_buffer_view(const BufferAccessor& buffer, MemoryType usage, size_t offset, size_t count, size_t stride, size_t alignment);
    inline size_t checked_view_offset(size_t base, size_t first, size_t count, size_t total, size_t stride) {
        if (!count || first > total || count > total - first)
            throw std::out_of_range("Typed buffer view requires a nonempty in-bounds range");
        if (!stride || first > (std::numeric_limits<size_t>::max() - base) / stride)
            throw std::overflow_error("Typed buffer view offset overflow");
        return base + first * stride;
    }
} // namespace VSL_NAMESPACE::typed::detail
