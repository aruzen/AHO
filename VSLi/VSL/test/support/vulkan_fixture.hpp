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

    std::filesystem::path shader_path(const char* name);
    std::vector<char> read_binary(const std::filesystem::path& path);

    // GLFW/surfaceを必要としないGPU環境。小さいfeature集合で初期化し、公開APIを検証する。
    class VulkanIntegrationTest : public testing::Test {
        std::vector<std::shared_ptr<vsl::impl::CommandPoolImpl>> _command_pools;
        VkDebugUtilsMessengerEXT _validation_messenger = VK_NULL_HANDLE;
        std::vector<std::string> _validation_errors;
        std::mutex _validation_mutex;
        bool _abort_validation_errors = false;
        std::string _expected_abort_vuid;
        std::size_t _expected_abort_count = 0;
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                            const VkDebugUtilsMessengerCallbackDataEXT* data, void* user);

    protected:
        bool has_validation_errors();
        // テスト専用の検証エラー注入。アプリケーションのコールバックはVK_FALSEを返す。
        void expect_validation_abort(const char* vuid);
        std::size_t validation_abort_count();
        virtual bool validate_vulkan() const;
        virtual bool require_timeline_semaphore() const;
        std::shared_ptr<vsl::impl::VulkanImpl<false>> instance;
        vsl::PhysicalDevice physical{std::shared_ptr<vsl::impl::PhysicalDeviceImpl>{}};
        vsl::LogicalDeviceAccessor device;
        std::uint32_t queue_family = 0;
        bool require_validation = false;

        void SetUp() override;
        void TearDown() override;
        vsl::CommandManager make_manager();
        vsl::CommandPool make_pool();
        vsl::PipelineLayoutEditAccessor make_layout_state();
        bool host_coherent_available() const;
    };

    inline std::filesystem::path shader_path(const char* name) {
        return std::filesystem::path(VSL_TEST_SHADER_DIR) / name;
    }

    inline std::vector<char> read_binary(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw std::runtime_error("Cannot read test fixture: " + path.string());
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    inline VKAPI_ATTR VkBool32 VKAPI_CALL
    VulkanIntegrationTest::ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                               const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            auto& self = *static_cast<VulkanIntegrationTest*>(user);
            std::lock_guard lock(self._validation_mutex);
            if (!self._expected_abort_vuid.empty() && data->pMessageIdName &&
                self._expected_abort_vuid == data->pMessageIdName)
                ++self._expected_abort_count;
            else
                self._validation_errors.emplace_back(data->pMessage);
            return self._abort_validation_errors ? VK_TRUE : VK_FALSE;
        }
        return VK_FALSE;
    }

    inline bool VulkanIntegrationTest::has_validation_errors() {
        std::lock_guard lock(_validation_mutex);
        return !_validation_errors.empty();
    }
    inline void VulkanIntegrationTest::expect_validation_abort(const char* vuid) {
        std::lock_guard lock(_validation_mutex);
        _expected_abort_vuid = vuid;
        _expected_abort_count = 0;
        _abort_validation_errors = true;
    }
    inline std::size_t VulkanIntegrationTest::validation_abort_count() {
        std::lock_guard lock(_validation_mutex);
        return _expected_abort_count;
    }
    inline bool VulkanIntegrationTest::validate_vulkan() const {
        return false;
    }
    inline bool VulkanIntegrationTest::require_timeline_semaphore() const {
        return false;
    }

    inline void VulkanIntegrationTest::SetUp() {
        std::vector<const char*> instance_extensions;
        vk::InstanceCreateInfo instance_info{};
        const auto extensions = vk::enumerateInstanceExtensionProperties();
        for (const auto& extension : extensions)
            for (const auto* name : {"VK_KHR_get_physical_device_properties2", "VK_KHR_surface", "VK_EXT_metal_surface", "VK_KHR_xcb_surface",
                                     "VK_KHR_xlib_surface", "VK_KHR_wayland_surface", "VK_KHR_win32_surface"})
                if (std::strcmp(extension.extensionName, name) == 0)
                    instance_extensions.push_back(name);
        if (std::ranges::any_of(extensions, [](const auto& extension) {
                return std::strcmp(extension.extensionName, "VK_KHR_portability_enumeration") == 0;
            })) {
            instance_extensions.push_back("VK_KHR_portability_enumeration");
            instance_info.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }
        instance_info.enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size());
        instance_info.ppEnabledExtensionNames = instance_extensions.data();
        const char* validation_layer = "VK_LAYER_KHRONOS_validation";
        VkDebugUtilsMessengerCreateInfoEXT debug_info{};
        const bool validation_enabled = require_validation || validate_vulkan();
        if (validation_enabled) {
            const auto layers = vk::enumerateInstanceLayerProperties();
            if (!std::ranges::any_of(layers, [&](const auto& layer) {
                    return std::strcmp(layer.layerName, validation_layer) == 0;
                }))
                GTEST_SKIP() << "Validation layer required for composition tests";
            if (!std::ranges::any_of(extensions, [](const auto& extension) {
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
            debug_info.pfnUserCallback = ValidationCallback;
            debug_info.pUserData = this;
            instance_info.pNext = &debug_info;
        }
        vk::Instance raw_instance;
        const auto result = vk::createInstance(&instance_info, nullptr, &raw_instance);
        if (result == vk::Result::eErrorIncompatibleDriver)
            GTEST_SKIP() << "Vulkan driver/GPU unavailable: " << vk::to_string(result);
        ASSERT_EQ(result, vk::Result::eSuccess);
        instance = std::make_shared<vsl::impl::VulkanImpl<false>>();
        instance->instance = raw_instance;
        if (validation_enabled) {
            auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(raw_instance, "vkCreateDebugUtilsMessengerEXT"));
            ASSERT_NE(create, nullptr);
            ASSERT_EQ(create(raw_instance, &debug_info, nullptr, &_validation_messenger), VK_SUCCESS);
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

        physical.data_ = std::make_shared<vsl::impl::PhysicalDeviceImpl>();
        physical.data_->device = selected;
        physical.data_->vulkan = instance;
        physical.data_->props = selected.getProperties();
        physical.data_->features = selected.getFeatures();
        physical.data_->mem_props = selected.getMemoryProperties();

        std::vector<const char*> device_extensions;
        for (const auto& extension : selected.enumerateDeviceExtensionProperties())
            for (const auto* name : {"VK_KHR_portability_subset", "VK_KHR_swapchain"})
                if (std::strcmp(extension.extensionName, name) == 0)
                    device_extensions.push_back(name);
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
        if (require_timeline_semaphore()) {
            const auto available = selected.enumerateDeviceExtensionProperties();
            if (!std::ranges::any_of(available, [](const auto& extension) {
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
        features.samplerAnisotropy = physical.data_->features->samplerAnisotropy;
        features.independentBlend = physical.data_->features->independentBlend;
        vk::DeviceCreateInfo device_info{};
        if (require_timeline_semaphore())
            device_info.pNext = &timeline;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size());
        device_info.ppEnabledExtensionNames = device_extensions.data();
        device_info.pEnabledFeatures = &features;
        const auto raw_device = selected.createDevice(device_info);
        auto data = std::make_shared<vsl::impl::LogicalDeviceImpl>();
        data->parent_device = physical.data_;
        data->graphics_family = data->present_family = queue_family;
        data->device = raw_device;
        device.data_ = std::move(data);
    }

    inline void VulkanIntegrationTest::TearDown() {
        if (device.data_ && device.data_->device) {
            device.data_->device.waitIdle();
            // GPUの完了後に、テストで保持していたコマンドプールを回収する。
            for (auto& pool : _command_pools) {
                device.data_->device.destroyCommandPool(pool->command_pool);
                pool->command_pool = nullptr;
            }
        }
        _command_pools.clear();
        device.data_.reset();
        physical.data_.reset();
        if (_validation_messenger) {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance->instance, "vkDestroyDebugUtilsMessengerEXT"));
            destroy(instance->instance, _validation_messenger, nullptr);
            _validation_messenger = VK_NULL_HANDLE;
        }
        instance.reset();
        for (const auto& error : _validation_errors)
            ADD_FAILURE() << error;
        if (!_expected_abort_vuid.empty())
            EXPECT_EQ(_expected_abort_count, 1u) << "Expected exactly one injected validation abort: " << _expected_abort_vuid;
    }

    inline vsl::CommandManager VulkanIntegrationTest::make_manager() {
        vsl::CommandManager manager(device);
        _command_pools.push_back(manager.data_->command_pool);
        return manager;
    }

    inline vsl::CommandPool VulkanIntegrationTest::make_pool() {
        vsl::CommandPool pool(device);
        _command_pools.push_back(pool.data_);
        return pool;
    }

    inline vsl::PipelineLayoutEditAccessor VulkanIntegrationTest::make_layout_state() {
        auto data = std::make_shared<vsl::impl::PipelineLayoutImpl>();
        data->device = device.data_;
        data->info = std::make_shared<vsl::impl::CreateInfo>();
        return {{std::move(data)}};
    }

    inline bool VulkanIntegrationTest::host_coherent_available() const {
        const auto& properties = *physical.data_->mem_props;
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((properties.memoryTypes[i].propertyFlags &
                 (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)) ==
                (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
                return true;
        return false;
    }

} // namespace vsl_test
