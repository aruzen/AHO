#pragma once
#include "resource_layout.hpp"
#include <algorithm>

namespace VSL_NAMESPACE::storage {
    class SwapchainLayout {
        size_t _count, _images_begin, _stride, _bytes, _alignment;

    public:
        constexpr explicit SwapchainLayout(size_t image_count);
        constexpr size_t images() const { return _count; }
        constexpr size_t bytes() const { return _bytes; }
        constexpr size_t alignment() const { return _alignment; }
        constexpr size_t image_offset(size_t index) const;
        constexpr void require_region(size_t bytes, size_t alignment) const;
    };

    constexpr SwapchainLayout::SwapchainLayout(size_t image_count) : _count(image_count), _images_begin(0), _stride(0), _bytes(0), _alignment(1) {
        if (!image_count || image_count > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Swapchain image count must fit a nonzero uint32");
        using Image = ResourceSlotLayout<impl::ImageImpl>;
        _alignment = std::max(SWAPCHAIN_SLOT_ALIGNMENT, Image::ALIGNMENT);
        _images_begin = checked_align(SWAPCHAIN_SLOT_SIZE, Image::ALIGNMENT);
        _stride = checked_align(Image::SIZE, Image::ALIGNMENT);
        if (image_count > std::numeric_limits<size_t>::max() / _stride)
            throw std::overflow_error("Swapchain CPU image placement overflow");
        _bytes = checked_align(checked_add(_images_begin, image_count * _stride), _alignment);
        if (!std::is_constant_evaluated())
            detail::require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>{});
    }
    constexpr size_t SwapchainLayout::image_offset(size_t index) const {
        if (index >= _count)
            throw std::out_of_range("Swapchain image slot index");
        return _images_begin + index * _stride;
    }
    constexpr void SwapchainLayout::require_region(size_t bytes, size_t alignment) const {
        if (!std::is_constant_evaluated())
            detail::require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>{});
        (void)checked_align(0, alignment);
        if (bytes < _bytes || alignment < _alignment)
            throw std::invalid_argument("CPU region does not satisfy the swapchain layout");
    }
} // namespace VSL_NAMESPACE::storage
