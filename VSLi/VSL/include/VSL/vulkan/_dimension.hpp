#pragma once
#include "../define.hpp"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "../dimension.hpp"

namespace VSL_NAMESPACE::helper::dimension {
    template<typename D>
    consteval vk::ImageViewType to_image_view_type() {
        static_assert(!is_dimension<D> || is_image_dimension<D>,
                      "GPU images support only one, two or three dimensions");
        if constexpr (std::same_as<VSL_NAMESPACE::D1, D>) {
            return vk::ImageViewType::e1D;
        } else if constexpr (std::same_as<VSL_NAMESPACE::D2, D>) {
            return vk::ImageViewType::e2D;
        } else if constexpr (std::same_as<VSL_NAMESPACE::D3, D>) {
            return vk::ImageViewType::e3D;
        }
        return vk::ImageViewType::___VSL_JOIN3(e, VSL_DEFAULT_dimension, D);
    }
} // namespace VSL_NAMESPACE::helper::dimension
