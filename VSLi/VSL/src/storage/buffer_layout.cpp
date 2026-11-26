#include <VSL/storage/buffer_layout.hpp>
#include <VSL/lifetime/buffer_storage.hpp>

namespace VSL_NAMESPACE::storage {
    static_assert(sizeof(impl::BufferImpl) == BUFFER_IMPL_SIZE);
    static_assert(alignof(impl::BufferImpl) == BUFFER_IMPL_ALIGNMENT);
    static_assert(sizeof(ResourceUseState) == BUFFER_USE_SIZE);
    static_assert(alignof(ResourceUseState) == BUFFER_USE_ALIGNMENT);
    static_assert(sizeof(lifetime::BufferSlotStorage) == BufferLayout::STRIDE);
    static_assert(alignof(lifetime::BufferSlotStorage) == BufferLayout::ALIGNMENT);
    static_assert(offsetof(lifetime::BufferSlotStorage, use) == BufferLayout::USE_OFFSET);
    static_assert(offsetof(lifetime::BufferSlotStorage, buffer) == 0);
    namespace detail {
        void require_buffer_layout(std::integral_constant<std::uint64_t, BUFFER_LAYOUT_ABI>) {}
    } // namespace detail
} // namespace VSL_NAMESPACE::storage
