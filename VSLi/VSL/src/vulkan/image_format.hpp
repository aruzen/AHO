#pragma once
#include <vulkan/vulkan.hpp>
#include <cstddef>
namespace vsl::_impl::helper {
inline vk::ImageAspectFlags imageAspects(vk::Format format) {
    switch (format) {
    case vk::Format::eD16Unorm:
    case vk::Format::eX8D24UnormPack32:
    case vk::Format::eD32Sfloat:
        return vk::ImageAspectFlagBits::eDepth;
    case vk::Format::eS8Uint:
        return vk::ImageAspectFlagBits::eStencil;
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}
// ClearValueの型の並びは、浮動小数点色、符号付き整数色、符号なし整数色、深度・ステンシル。
inline std::size_t clearKind(vk::Format format) {
    if (imageAspects(format) != vk::ImageAspectFlagBits::eColor)
        return 3;
    switch (format) {
    case vk::Format::eR8Sint:
    case vk::Format::eR8G8Sint:
    case vk::Format::eR8G8B8Sint:
    case vk::Format::eB8G8R8Sint:
    case vk::Format::eR8G8B8A8Sint:
    case vk::Format::eB8G8R8A8Sint:
    case vk::Format::eA8B8G8R8SintPack32:
    case vk::Format::eR16Sint:
    case vk::Format::eR16G16Sint:
    case vk::Format::eR16G16B16Sint:
    case vk::Format::eR16G16B16A16Sint:
    case vk::Format::eR32Sint:
    case vk::Format::eR32G32Sint:
    case vk::Format::eR32G32B32Sint:
    case vk::Format::eR32G32B32A32Sint:
    case vk::Format::eR64Sint:
    case vk::Format::eR64G64Sint:
    case vk::Format::eR64G64B64Sint:
    case vk::Format::eR64G64B64A64Sint:
        return 1;
    case vk::Format::eR8Uint:
    case vk::Format::eR8G8Uint:
    case vk::Format::eR8G8B8Uint:
    case vk::Format::eB8G8R8Uint:
    case vk::Format::eR8G8B8A8Uint:
    case vk::Format::eB8G8R8A8Uint:
    case vk::Format::eA8B8G8R8UintPack32:
    case vk::Format::eR16Uint:
    case vk::Format::eR16G16Uint:
    case vk::Format::eR16G16B16Uint:
    case vk::Format::eR16G16B16A16Uint:
    case vk::Format::eR32Uint:
    case vk::Format::eR32G32Uint:
    case vk::Format::eR32G32B32Uint:
    case vk::Format::eR32G32B32A32Uint:
    case vk::Format::eR64Uint:
    case vk::Format::eR64G64Uint:
    case vk::Format::eR64G64B64Uint:
    case vk::Format::eR64G64B64A64Uint:
        return 2;
    default:
        return 0;
    }
}
}
