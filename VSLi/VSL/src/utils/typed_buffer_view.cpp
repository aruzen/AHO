#include <VSL/utils/typed_buffer_view.hpp>

namespace VSL_NAMESPACE::typed::detail {
    void validate_buffer_view(const BufferAccessor& buffer, MemoryType usage, size_t offset, size_t count, size_t stride, size_t alignment) {
        if (!buffer.data_ || !contain(buffer.get_usage(), usage))
            throw std::invalid_argument("Typed buffer view requires matching buffer usage");
        if (!count || offset > buffer.requested_size() || !stride || count > (buffer.requested_size() - offset) / stride)
            throw std::out_of_range("Typed buffer view exceeds requested storage");
        if (!alignment || offset % alignment)
            throw std::invalid_argument("Typed buffer view offset violates payload alignment");
    }
} // namespace VSL_NAMESPACE::typed::detail
