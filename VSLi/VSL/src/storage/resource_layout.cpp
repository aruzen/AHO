#include <VSL/storage/resource_layout.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/sampler_storage.hpp>

namespace VSL_NAMESPACE::storage {
    static_assert(sizeof(impl::BufferImpl) == ImplLayout<impl::BufferImpl>::SIZE);
    static_assert(alignof(impl::BufferImpl) == ImplLayout<impl::BufferImpl>::ALIGNMENT);
    static_assert(sizeof(impl::ImageImpl) == ImplLayout<impl::ImageImpl>::SIZE);
    static_assert(alignof(impl::ImageImpl) == ImplLayout<impl::ImageImpl>::ALIGNMENT);
    static_assert(sizeof(impl::SamplerImpl) == ImplLayout<impl::SamplerImpl>::SIZE);
    static_assert(alignof(impl::SamplerImpl) == ImplLayout<impl::SamplerImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::BufferSlotStorage) == ResourceSlotLayout<impl::BufferImpl>::SIZE);
    static_assert(alignof(lifetime::BufferSlotStorage) == ResourceSlotLayout<impl::BufferImpl>::ALIGNMENT);
    static_assert(offsetof(lifetime::BufferSlotStorage, use) == ResourceSlotLayout<impl::BufferImpl>::USE_OFFSET);
    static_assert(offsetof(lifetime::BufferSlotStorage, buffer) == 0);
    static_assert(sizeof(lifetime::ImageSlotStorage) == ResourceSlotLayout<impl::ImageImpl>::SIZE);
    static_assert(alignof(lifetime::ImageSlotStorage) == ResourceSlotLayout<impl::ImageImpl>::ALIGNMENT);
    static_assert(offsetof(lifetime::ImageSlotStorage, use) == ResourceSlotLayout<impl::ImageImpl>::USE_OFFSET);
    static_assert(offsetof(lifetime::ImageSlotStorage, image) == 0);
    static_assert(sizeof(lifetime::SamplerSlotStorage) == ResourceSlotLayout<impl::SamplerImpl>::SIZE);
    static_assert(alignof(lifetime::SamplerSlotStorage) == ResourceSlotLayout<impl::SamplerImpl>::ALIGNMENT);
    static_assert(offsetof(lifetime::SamplerSlotStorage, use) == ResourceSlotLayout<impl::SamplerImpl>::USE_OFFSET);
    static_assert(offsetof(lifetime::SamplerSlotStorage, sampler) == 0);
    static_assert(BufferLayout::STRIDE == ResourceSlotLayout<impl::BufferImpl>::SIZE);
    namespace detail {
        void require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>) {}
    } // namespace detail
} // namespace VSL_NAMESPACE::storage
