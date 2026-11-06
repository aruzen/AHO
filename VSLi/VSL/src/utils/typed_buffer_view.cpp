#include <VSL/utils/typed_buffer_view.hpp>

namespace VSL_NAMESPACE::typed::detail {
    void validate_buffer_view(const BufferAccessor& buffer, MemoryType usage, size_t offset, size_t count, size_t stride, size_t alignment) {
        if (!buffer.data_)
            throw std::invalid_argument("Typed buffer view requires a buffer");
        validate_buffer_view(BufferView::FromBuffer(buffer), usage, offset, count, stride, alignment);
    }
    void validate_buffer_view(const BufferView& buffer, MemoryType usage, size_t offset, size_t count, size_t stride, size_t alignment) {
        if (!contain(buffer.ref().usage(), usage))
            throw std::invalid_argument("Typed buffer view requires matching buffer usage");
        if (!count || offset > buffer.size() || !stride || count > (buffer.size() - offset) / stride)
            throw std::out_of_range("Typed buffer view exceeds requested storage");
        if (!alignment || (buffer.byte_offset() + offset) % alignment)
            throw std::invalid_argument("Typed buffer view offset violates payload alignment");
    }
} // namespace VSL_NAMESPACE::typed::detail
