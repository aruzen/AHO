#pragma once

#include <gtest/gtest.h>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

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

  protected:
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
            for (const auto *name : {"VK_KHR_surface", "VK_EXT_metal_surface", "VK_KHR_xcb_surface",
                                     "VK_KHR_xlib_surface", "VK_KHR_wayland_surface", "VK_KHR_win32_surface"})
                if (std::strcmp(extension.extensionName, name) == 0)
                    instance_extensions.push_back(name);
        if (std::ranges::any_of(extensions, [](const auto &extension) {
                return std::strcmp(extension.extensionName, "VK_KHR_portability_enumeration") == 0;
            })) {
            instance_extensions.push_back("VK_KHR_portability_enumeration");
            instance_info.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }
        const char* validation_layer = "VK_LAYER_KHRONOS_validation";
        if (require_validation) {
            const auto layers = vk::enumerateInstanceLayerProperties();
            if (!std::ranges::any_of(layers, [&](const auto& layer) {
                    return std::strcmp(layer.layerName, validation_layer) == 0;
                }))
                GTEST_SKIP() << "VK_LAYER_KHRONOS_validation is required";
            if (!std::ranges::any_of(extensions, [](const auto& extension) {
                    return std::strcmp(extension.extensionName, "VK_EXT_debug_utils") == 0;
                }))
                GTEST_SKIP() << "VK_EXT_debug_utils is required";
            instance_extensions.push_back("VK_EXT_debug_utils");
            instance_info.enabledLayerCount = 1;
            instance_info.ppEnabledLayerNames = &validation_layer;
        }
        instance_info.enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size());
        instance_info.ppEnabledExtensionNames = instance_extensions.data();
        vk::Instance raw_instance;
        const auto result = vk::createInstance(&instance_info, nullptr, &raw_instance);
        if (result == vk::Result::eErrorIncompatibleDriver)
            GTEST_SKIP() << "Vulkan driver/GPU unavailable: " << vk::to_string(result);
        ASSERT_EQ(result, vk::Result::eSuccess);
        instance = std::make_shared<vsl::_impl::Vulkan_impl<false>>();
        instance->instance = raw_instance;

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
        const float priority = 1.0f;
        vk::DeviceQueueCreateInfo queue_info{};
        queue_info.queueFamilyIndex = queue_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;
        vk::PhysicalDeviceFeatures features{};
        features.samplerAnisotropy = physical._data->features->samplerAnisotropy;
        features.independentBlend = physical._data->features->independentBlend;
        vk::DeviceCreateInfo device_info{};
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
        instance.reset();
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
