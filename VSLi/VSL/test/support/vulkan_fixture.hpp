#pragma once

#include <gtest/gtest.h>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <mutex>

namespace vsl_test {

inline std::filesystem::path shader_path(const char *name) {
    return std::filesystem::path(VSL_TEST_SHADER_DIR) / name;
}

inline std::vector<char> read_binary(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Cannot read test fixture: " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

// GLFW/surface を必要としない GPU 環境。VSL が要求するより小さい feature 集合で
// Vulkan を初期化し、検証対象の public API には通常の Accessor を渡す。
class VulkanIntegrationTest : public testing::Test {
    std::vector<std::shared_ptr<vsl::_impl::CommandPool_impl>> command_pools_;
    VkDebugUtilsMessengerEXT validation_messenger_ = VK_NULL_HANDLE;
    std::vector<std::string> validation_errors_;
    std::mutex validation_mutex_;
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                        const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            auto &self = *static_cast<VulkanIntegrationTest *>(user);
            std::lock_guard lock(self.validation_mutex_);
            self.validation_errors_.emplace_back(data->pMessage);
        }
        return VK_FALSE;
    }

  protected:
    virtual bool validate_vulkan() const { return false; }
    virtual bool require_timeline_semaphore() const { return false; }
    std::shared_ptr<vsl::_impl::Vulkan_impl<false>> instance;
    vsl::PhysicalDevice physical{std::shared_ptr<vsl::_impl::PhysicalDevice_impl>{}};
    vsl::LogicalDeviceAccessor device;
    std::uint32_t queue_family = 0;
    bool require_validation = false;

    void SetUp() override {
        std::vector<const char *> instance_extensions;
        vk::InstanceCreateInfo instance_info{};
        const auto extensions = vk::enumerateInstanceExtensionProperties();
        for (const auto &extension : extensions)
            for (const auto *name : {"VK_KHR_get_physical_device_properties2", "VK_KHR_surface", "VK_EXT_metal_surface", "VK_KHR_xcb_surface",
                                     "VK_KHR_xlib_surface", "VK_KHR_wayland_surface", "VK_KHR_win32_surface"})
                if (std::strcmp(extension.extensionName, name) == 0)
                    instance_extensions.push_back(name);
        if (std::ranges::any_of(extensions, [](const auto &extension) {
                return std::strcmp(extension.extensionName, "VK_KHR_portability_enumeration") == 0;
            })) {
            instance_extensions.push_back("VK_KHR_portability_enumeration");
            instance_info.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }
        instance_info.enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size());
        instance_info.ppEnabledExtensionNames = instance_extensions.data();
        const char *validation_layer = "VK_LAYER_KHRONOS_validation";
        VkDebugUtilsMessengerCreateInfoEXT debug_info{};
        const bool validationEnabled = require_validation || validate_vulkan();
        if (validationEnabled) {
            const auto layers = vk::enumerateInstanceLayerProperties();
            if (!std::ranges::any_of(layers, [&](const auto &layer) {
                    return std::strcmp(layer.layerName, validation_layer) == 0;
                }))
                GTEST_SKIP() << "Validation layer required for composition tests";
            if (!std::ranges::any_of(extensions, [](const auto &extension) {
                    return std::strcmp(extension.extensionName, "VK_EXT_debug_utils") == 0;
                }))
                GTEST_SKIP() << "VK_EXT_debug_utils is required";
            instance_extensions.push_back("VK_EXT_debug_utils");
            instance_info.enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size());
            instance_info.ppEnabledExtensionNames = instance_extensions.data();
            instance_info.enabledLayerCount = 1;
            instance_info.ppEnabledLayerNames = &validation_layer;
            debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            debug_info.pfnUserCallback = validation_callback;
            debug_info.pUserData = this;
            instance_info.pNext = &debug_info;
        }
        vk::Instance raw_instance;
        const auto result = vk::createInstance(&instance_info, nullptr, &raw_instance);
        if (result == vk::Result::eErrorIncompatibleDriver)
            GTEST_SKIP() << "Vulkan driver/GPU unavailable: " << vk::to_string(result);
        ASSERT_EQ(result, vk::Result::eSuccess);
        instance = std::make_shared<vsl::_impl::Vulkan_impl<false>>();
        instance->instance = raw_instance;
        if (validationEnabled) {
            auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(raw_instance, "vkCreateDebugUtilsMessengerEXT"));
            ASSERT_NE(create, nullptr);
            ASSERT_EQ(create(raw_instance, &debug_info, nullptr, &validation_messenger_), VK_SUCCESS);
        }

        vk::PhysicalDevice selected;
        for (const auto candidate : raw_instance.enumeratePhysicalDevices()) {
            const auto queues = candidate.getQueueFamilyProperties();
            for (std::uint32_t i = 0; i < queues.size(); ++i) {
                if ((queues[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    (queues[i].queueFlags & vk::QueueFlagBits::eCompute)) {
                    selected = candidate;
                    queue_family = i;
                    break;
                }
            }
            if (selected)
                break;
        }
        if (!selected)
            GTEST_SKIP() << "No device has a graphics and compute queue";

        physical._data = std::make_shared<vsl::_impl::PhysicalDevice_impl>();
        physical._data->device = selected;
        physical._data->vulkan = instance;
        physical._data->props = selected.getProperties();
        physical._data->features = selected.getFeatures();
        physical._data->memProps = selected.getMemoryProperties();

        std::vector<const char *> device_extensions;
        for (const auto &extension : selected.enumerateDeviceExtensionProperties())
            for (const auto *name : {"VK_KHR_portability_subset", "VK_KHR_swapchain"})
                if (std::strcmp(extension.extensionName, name) == 0)
                    device_extensions.push_back(name);
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
        if (require_timeline_semaphore()) {
            const auto available = selected.enumerateDeviceExtensionProperties();
            if (!std::ranges::any_of(available, [](const auto &extension) {
                    return std::strcmp(extension.extensionName, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0;
                }))
                GTEST_SKIP() << "Timeline semaphore extension unavailable";
            auto get_features = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(
                vkGetInstanceProcAddr(raw_instance, "vkGetPhysicalDeviceFeatures2KHR"));
            ASSERT_NE(get_features, nullptr);
            VkPhysicalDeviceFeatures2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            query.pNext = &timeline;
            get_features(selected, &query);
            if (!timeline.timelineSemaphore)
                GTEST_SKIP() << "Timeline semaphore feature unavailable";
            device_extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        }
        const float priority = 1.0f;
        vk::DeviceQueueCreateInfo queue_info{};
        queue_info.queueFamilyIndex = queue_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;
        vk::PhysicalDeviceFeatures features{};
        features.samplerAnisotropy = physical._data->features->samplerAnisotropy;
        features.independentBlend = physical._data->features->independentBlend;
        vk::DeviceCreateInfo device_info{};
        if (require_timeline_semaphore())
            device_info.pNext = &timeline;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size());
        device_info.ppEnabledExtensionNames = device_extensions.data();
        device_info.pEnabledFeatures = &features;
        const auto raw_device = selected.createDevice(device_info);
        auto data = std::make_shared<vsl::_impl::LogicalDevice_impl>();
        data->parentDevice = physical._data;
        data->graphicsFamily = data->presentFamily = queue_family;
        data->device = raw_device;
        device._data = std::move(data);
    }

    void TearDown() override {
        if (device._data && device._data->device) {
            device._data->device.waitIdle();
            // CommandPool_impl に破棄処理がない既知の不具合 I23 のため、
            // テストが作成した pool は GPU 完了・buffer 解放後に明示的に回収する。
            for (auto &pool : command_pools_) {
                device._data->device.destroyCommandPool(pool->commandPool);
                pool->commandPool = nullptr;
            }
        }
        command_pools_.clear();
        device._data.reset();
        physical._data.reset();
        if (validation_messenger_) {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance->instance, "vkDestroyDebugUtilsMessengerEXT"));
            destroy(instance->instance, validation_messenger_, nullptr);
            validation_messenger_ = VK_NULL_HANDLE;
        }
        instance.reset();
        for (const auto &error : validation_errors_)
            ADD_FAILURE() << error;
    }

    vsl::CommandManager make_manager() {
        vsl::CommandManager manager(device);
        command_pools_.push_back(manager._data->commandPool);
        return manager;
    }

    vsl::CommandPool make_pool() {
        vsl::CommandPool pool(device);
        command_pools_.push_back(pool._data);
        return pool;
    }

    vsl::PipelineLayoutAccessor make_layout_state() {
        auto data = std::make_shared<vsl::_impl::PipelineLayout_impl>();
        data->device = device._data;
        data->info = std::make_shared<vsl::_impl::CreateInfo>();
        return {std::move(data)};
    }

    bool host_coherent_available() const {
        const auto &properties = *physical._data->memProps;
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((properties.memoryTypes[i].propertyFlags &
                 (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)) ==
                (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
                return true;
        return false;
    }
};

} // namespace vsl_test
