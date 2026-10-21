#pragma once
#include "../define.hpp"

#include "../utils/string.hpp"
#include <vector>
#include <any>

#include "../test.hpp"
#include "pv.hpp"

#include <memory>
#include <concepts>

namespace VSL_NAMESPACE {
    struct PhysicalDevice;
    struct PhysicalDevicesSercher;

    template<typename S = VSL_NAMESPACE::PhysicalDevicesSercher>
    struct PhysicalDevices;

    static std::vector<const char*> VALIDATION_EXTENSIONS = {
        "VK_EXT_debug_utils",
        // "VK_EXT_layer_settings"
    };

    static std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"};

    struct VulkanAccessor {
        std::shared_ptr<impl::VulkanImplAccessor> accessor;
    };

    template<bool validation = VSL_NAMESPACE::VALIDATION>
    struct Vulkan : VulkanAccessor {
    public:
        bool check_validation_layer_support();

        Vulkan(const char* app_name, const std::vector<const char*>& require_extensions = {});
        ~Vulkan();

        template<typename S = VSL_NAMESPACE::PhysicalDevicesSercher>
        PhysicalDevices<S> devices();

#ifdef VSL_NAMESPACE_TEST
        void test();
#endif // VSL_NAMESPACE_TEST
        std::shared_ptr<impl::VulkanImpl<validation>> data_;
    };
} // namespace VSL_NAMESPACE
