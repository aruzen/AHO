#pragma once
#include <VSL/operation/pipeline_layout_ops.hpp>
#include <VSL/lifetime/instance_storage.hpp>
#include <VSL/lifetime/physical_device_storage.hpp>

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
        std::vector<std::string> _validation_errors;
        std::mutex _validation_mutex;
        bool _abort_validation_errors = false;
        std::string _expected_abort_vuid;
        std::size_t _expected_abort_count = 0;
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                           const VkDebugUtilsMessengerCallbackDataEXT* data, void* user);
        static VkResult CreateInstance(void* context, const VkInstanceCreateInfo* info, VkInstance* output);
        static VkResult CreateMessenger(void* context, VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* info, VkDebugUtilsMessengerEXT* output);

    protected:
        bool has_validation_errors();
        // テスト専用の検証エラー注入。アプリケーションのコールバックはVK_FALSEを返す。
        void expect_validation_abort(const char* vuid);
        std::size_t validation_abort_count();
        virtual bool validate_vulkan() const;
        virtual bool require_timeline_semaphore() const;
        virtual vsl::PresentFencePolicy present_fence_policy() const;
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

    struct LayoutInspection : vsl::operation::PipelineScratch {
        std::vector<vk::DescriptorSetLayout> layouts;
        std::vector<vk::PushConstantRange> ranges;
        vk::PipelineLayoutCreateInfo pipeline_layout;
        explicit LayoutInspection(vsl::PipelineLayoutAccessor layout);
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

    inline VkResult VulkanIntegrationTest::CreateInstance(void* context, const VkInstanceCreateInfo* info, VkInstance* output) {
        auto actual = *info;
        VkDebugUtilsMessengerCreateInfoEXT debug{};
        if (info->pNext && static_cast<const VkBaseInStructure*>(info->pNext)->sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT) {
            debug = *static_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(info->pNext);
            debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            debug.pfnUserCallback = ValidationCallback;
            debug.pUserData = context;
            actual.pNext = &debug;
        }
        return vkCreateInstance(&actual, nullptr, output);
    }
    inline VkResult VulkanIntegrationTest::CreateMessenger(void* context, VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* info, VkDebugUtilsMessengerEXT* output) {
        auto actual = *info;
        actual.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        actual.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        actual.pfnUserCallback = ValidationCallback;
        actual.pUserData = context;
        const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        return create(instance, &actual, nullptr, output);
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
    inline vsl::PresentFencePolicy VulkanIntegrationTest::present_fence_policy() const {
        return vsl::PresentFencePolicy::LEGACY;
    }

    inline void VulkanIntegrationTest::SetUp() {
        vsl::InstanceCreateInfo instance_info;
        instance_info.application_name = "VSL GPU test";
        instance_info.api_version = VK_API_VERSION_1_0;
        const auto extensions = vk::enumerateInstanceExtensionProperties();
        for (const auto& extension : extensions)
            for (const auto* name : {"VK_KHR_get_physical_device_properties2", "VK_KHR_surface", "VK_EXT_metal_surface", "VK_KHR_xcb_surface",
                                     "VK_KHR_xlib_surface", "VK_KHR_wayland_surface", "VK_KHR_win32_surface", "VK_KHR_portability_enumeration"})
                if (std::strcmp(extension.extensionName, name) == 0)
                    instance_info.extensions.emplace_back(name);
        instance_info.validation = require_validation || validate_vulkan();
        if (present_fence_policy() == vsl::PresentFencePolicy::REQUIRE_FENCE) {
            for (const auto* name : {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                                     VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME}) {
                if (!std::ranges::any_of(extensions, [&](const auto& extension) { return std::strcmp(extension.extensionName, name) == 0; }))
                    GTEST_SKIP() << "Required present Fence Instance extension unavailable: " << name;
                if (std::ranges::find(instance_info.extensions, name) == instance_info.extensions.end())
                    instance_info.extensions.emplace_back(name);
            }
        }
        if (instance_info.validation) {
            const std::array<std::string, 1> layers{"VK_LAYER_KHRONOS_validation"};
            if (!vsl::operation::instance_layers_supported(layers))
                GTEST_SKIP() << "Validation layer required for composition tests";
            if (!std::ranges::any_of(extensions, [](const auto& extension) { return std::strcmp(extension.extensionName, "VK_EXT_debug_utils") == 0; }))
                GTEST_SKIP() << "VK_EXT_debug_utils is required";
        }
        try {
            const vsl::operation::InstanceNativeCalls calls{.context = this, .create_instance = CreateInstance, .create_messenger = CreateMessenger};
            instance = vsl::lifetime::make_heap_instance(std::move(instance_info), calls);
        } catch (const vk::IncompatibleDriverError& error) {
            GTEST_SKIP() << "Vulkan driver/GPU unavailable: " << error.what();
        }
        const auto raw_instance = instance->instance;
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

        const auto candidate = vsl::operation::inspect_physical_device(*instance, selected);
        physical.data_ = vsl::lifetime::make_heap_physical_device(vsl::VulkanAccessor{instance}, candidate);
        physical.data_->make_mem_props();

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
        vsl::DeviceCreateInfo device_info;
        device_info.graphics_family = queue_family;
        device_info.extensions.assign(device_extensions.begin(), device_extensions.end());
        if (physical.data_->features->samplerAnisotropy)
            device_info.features = device_info.features | vsl::DeviceFeature::SAMPLER_ANISOTROPY;
        if (physical.data_->features->independentBlend)
            device_info.features = device_info.features | vsl::DeviceFeature::INDEPENDENT_BLEND;
        if (require_timeline_semaphore())
            device_info.features = device_info.features | vsl::DeviceFeature::TIMELINE_SEMAPHORE;
        if (present_fence_policy() == vsl::PresentFencePolicy::REQUIRE_FENCE) {
            if (!vsl::has_device_features(physical.supported_features(), vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1))
                GTEST_SKIP() << "Present Fence maintenance1 feature unavailable";
            device_info.features = device_info.features | vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1;
        }
        device = vsl::LogicalDeviceAccessor::Create(physical, std::move(device_info));
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

    inline LayoutInspection::LayoutInspection(vsl::PipelineLayoutAccessor layout) : PipelineScratch(layout.recipe().graphics_state) {
        for (const auto* binding : layout.recipe().binding_layouts)
            layouts.push_back(binding->layout);
        for (const auto& range : layout.recipe().push_ranges)
            ranges.push_back({.stageFlags = vk::ShaderStageFlags(static_cast<std::uint32_t>(range.target_shader)),
                              .offset = range.offset,
                              .size = range.size});
        pipeline_layout.setLayoutCount = static_cast<std::uint32_t>(layouts.size());
        pipeline_layout.pSetLayouts = layouts.empty() ? nullptr : layouts.data();
        pipeline_layout.pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size());
        pipeline_layout.pPushConstantRanges = ranges.empty() ? nullptr : ranges.data();
    }
    inline LayoutInspection inspect_layout(vsl::PipelineLayoutAccessor layout) {
        return LayoutInspection(std::move(layout));
    }
    inline vsl::PipelineLayoutEditAccessor VulkanIntegrationTest::make_layout_state() {
        struct LayoutState : vsl::PipelineLayoutEditAccessor {
            explicit LayoutState(vsl::LogicalDeviceAccessor device) { init_start(device); }
        };
        return LayoutState(device);
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
