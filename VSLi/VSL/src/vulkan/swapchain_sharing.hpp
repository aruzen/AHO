#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan.hpp>
#include <cstdint>

namespace VSL_NAMESPACE::_impl::helper {
    // The queue-family array must remain alive until the native swapchain creation call returns.
    inline void configureSwapchainSharing(vk::SwapchainCreateInfoKHR& info, const std::uint32_t (&families)[2],
                                          vk::SurfaceTransformFlagBitsKHR transform) {
        info.preTransform = transform;
        if (families[0] != families[1]) {
            info.imageSharingMode = vk::SharingMode::eConcurrent;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = families;
        } else {
            info.imageSharingMode = vk::SharingMode::eExclusive;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = nullptr;
        }
    }
} // namespace VSL_NAMESPACE::_impl::helper
