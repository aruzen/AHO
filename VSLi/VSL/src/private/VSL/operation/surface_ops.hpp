#pragma once
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::operation {
    struct SurfaceNativeCalls {
        void* context = nullptr;
        VkResult (*create)(void*, VkInstance, void*, VkSurfaceKHR*) = nullptr;
        void (*destroy)(void*, VkInstance, VkSurfaceKHR) noexcept = nullptr;
    };
    void initialize_surface(impl::SurfaceImpl& target, impl::VulkanImplAccessor& instance, void* window, const SurfaceNativeCalls& calls = {});
    void destroy_surface(impl::SurfaceImpl& target, const SurfaceNativeCalls& calls = {}) noexcept;
} // namespace VSL_NAMESPACE::operation
