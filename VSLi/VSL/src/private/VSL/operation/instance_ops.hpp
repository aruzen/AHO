#pragma once
#include <VSL/operation/instance_create_info.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <span>

namespace VSL_NAMESPACE::operation {
    // 呼出し単位のnative差替え。未指定は実API、Impl/ownerには保存しない。
    struct InstanceNativeCalls {
        void* context = nullptr;
        VkResult (*create_instance)(void*, const VkInstanceCreateInfo*, VkInstance*) = nullptr;
        VkResult (*create_messenger)(void*, VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, VkDebugUtilsMessengerEXT*) = nullptr;
        void (*destroy_instance)(void*, VkInstance) noexcept = nullptr;
        void (*destroy_messenger)(void*, VkInstance, VkDebugUtilsMessengerEXT) noexcept = nullptr;
    };
    bool instance_layers_supported(std::span<const std::string> layers);
    void initialize_instance(impl::VulkanImplAccessor& instance, InstanceCreateInfo info, const InstanceNativeCalls& calls = {});
    void destroy_instance(impl::VulkanImplAccessor& instance, const InstanceNativeCalls& calls = {}) noexcept;
} // namespace VSL_NAMESPACE::operation
