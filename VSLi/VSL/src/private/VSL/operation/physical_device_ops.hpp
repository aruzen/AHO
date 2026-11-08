#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <span>

namespace VSL_NAMESPACE::operation {
    // originは借用。採点から構築までInstanceを保持し、未publish起動rootの外へ出さない。
    struct PhysicalDeviceCandidate {
        impl::VulkanImplAccessor* instance = nullptr;
        vk::PhysicalDevice device;
        vk::PhysicalDeviceProperties properties;
        vk::PhysicalDeviceFeatures features;
    };
    std::vector<vk::PhysicalDevice> enumerate_physical_devices(impl::VulkanImplAccessor& instance);
    PhysicalDeviceCandidate inspect_physical_device(impl::VulkanImplAccessor& instance, vk::PhysicalDevice device);
    PhysicalDeviceCandidate select_physical_device(impl::VulkanImplAccessor& instance);
    PhysicalDeviceCandidate select_physical_device(impl::VulkanImplAccessor& instance, std::span<const vk::PhysicalDevice> candidates);
    int score_physical_device(const vk::PhysicalDeviceProperties& properties, const vk::PhysicalDeviceFeatures& features);
    int rate_physical_device(vk::PhysicalDevice device);
    void initialize_physical_device(impl::PhysicalDeviceImpl& target, impl::VulkanImplAccessor& instance, const PhysicalDeviceCandidate& candidate);
    const vk::PhysicalDeviceProperties& physical_device_properties(impl::PhysicalDeviceImpl& device);
    vk::PhysicalDeviceProperties query_physical_device_properties(vk::PhysicalDevice device);
    vk::PhysicalDeviceMemoryProperties query_physical_device_memory(vk::PhysicalDevice device);
} // namespace VSL_NAMESPACE::operation
