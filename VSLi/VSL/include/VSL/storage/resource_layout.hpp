#pragma once
#include "buffer_layout.hpp"
#include "../vulkan/pv.hpp"

namespace VSL_NAMESPACE::storage {
    template<typename Impl>
    struct ImplLayout;
    template<>
    struct ImplLayout<impl::BufferImpl> {
        static constexpr size_t SIZE = BUFFER_IMPL_SIZE, ALIGNMENT = BUFFER_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::ImageImpl> {
        static constexpr size_t SIZE = IMAGE_IMPL_SIZE, ALIGNMENT = IMAGE_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::SamplerImpl> {
        static constexpr size_t SIZE = SAMPLER_IMPL_SIZE, ALIGNMENT = SAMPLER_IMPL_ALIGNMENT;
    };
    template<typename Impl>
    struct ResourceSlotLayout {
        static constexpr size_t USE_OFFSET = checked_align(ImplLayout<Impl>::SIZE, BUFFER_USE_ALIGNMENT);
        static constexpr size_t ALIGNMENT = ImplLayout<Impl>::ALIGNMENT > BUFFER_USE_ALIGNMENT ? ImplLayout<Impl>::ALIGNMENT : BUFFER_USE_ALIGNMENT;
        static constexpr size_t SIZE = checked_align(USE_OFFSET + BUFFER_USE_SIZE, ALIGNMENT);
    };
    enum class ResourceKind : std::uint32_t {
        BUFFER,
        IMAGE,
        SAMPLER
    };
    struct SlotLayout {
        size_t size, alignment, use_offset;
    };
    constexpr SlotLayout resource_slot_layout(ResourceKind kind);
    constexpr size_t checked_add(size_t first, size_t second);
    namespace detail {
        void require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>);
    } // namespace detail

    constexpr SlotLayout resource_slot_layout(ResourceKind kind) {
        using Buffer = ResourceSlotLayout<impl::BufferImpl>;
        using Image = ResourceSlotLayout<impl::ImageImpl>;
        using Sampler = ResourceSlotLayout<impl::SamplerImpl>;
        switch (kind) {
            case ResourceKind::BUFFER:
                return {Buffer::SIZE, Buffer::ALIGNMENT, Buffer::USE_OFFSET};
            case ResourceKind::IMAGE:
                return {Image::SIZE, Image::ALIGNMENT, Image::USE_OFFSET};
            case ResourceKind::SAMPLER:
                return {Sampler::SIZE, Sampler::ALIGNMENT, Sampler::USE_OFFSET};
        }
        throw std::invalid_argument("Unknown resource implementation kind");
    }
    constexpr size_t checked_add(size_t first, size_t second) {
        if (second > std::numeric_limits<size_t>::max() - first)
            throw std::overflow_error("Resource placement size overflow");
        return first + second;
    }
} // namespace VSL_NAMESPACE::storage
