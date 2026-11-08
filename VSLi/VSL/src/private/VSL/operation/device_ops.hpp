#pragma once
#include <VSL/operation/device_create_info.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::operation {
    struct DeviceNativeCalls {
        void* context = nullptr;
        VkResult (*create)(void*, VkPhysicalDevice, const VkDeviceCreateInfo*, VkDevice*) = nullptr;
        void (*destroy)(void*, VkDevice) noexcept = nullptr;
    };
    DeviceFeature supported_device_features(impl::PhysicalDeviceImpl& physical);
    DeviceCreateInfo graphics_device_info(impl::PhysicalDeviceImpl& physical, impl::SurfaceImpl& surface, bool validation);
    void initialize_device(impl::LogicalDeviceImpl& target, impl::PhysicalDeviceImpl& physical, DeviceCreateInfo info,
                           const DeviceNativeCalls& calls = {});
    void destroy_device(impl::LogicalDeviceImpl& target, const DeviceNativeCalls& calls = {}) noexcept;
} // namespace VSL_NAMESPACE::operation
