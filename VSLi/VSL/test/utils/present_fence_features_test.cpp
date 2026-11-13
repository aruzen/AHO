#include <VSL/lifetime/startup_access.hpp>
#include "support/window_fixture.hpp"
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/lifetime/device_storage.hpp>
#include <VSL/operation/instance_ops.hpp>
#include <optional>

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

namespace {
    namespace l = vsl::lifetime;
    namespace o = vsl::operation;
    namespace s = vsl::storage;
    namespace t = vsl::typed;
    constexpr auto MAINTENANCE = vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1;
    constexpr auto TIMELINE = vsl::DeviceFeature::TIMELINE_SEMAPHORE;
    class PresentFenceFeaturesVulkanIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    struct FeatureProbe {
        unsigned creates = 0, timelines = 0, maintenance = 0;
        bool swapchain_extension = false, maintenance_extension = false;
        static VkResult Create(void* context, VkPhysicalDevice physical, const VkDeviceCreateInfo* info, VkDevice* output);
    };
    struct DeviceSlot {
        alignas(l::DeviceSlotStorage) std::byte memory[sizeof(l::DeviceSlotStorage)];
        bool constructed = false;
        ~DeviceSlot();
        void construct(vsl::PhysicalDevice physical, vsl::DeviceCreateInfo info, FeatureProbe& probe);
        vsl::impl::LogicalDeviceImpl& value() { return l::device_at(memory); }
    };
    std::optional<vsl::VulkanAccessor> strict_instance(std::uint32_t api = VK_API_VERSION_1_3);

    VkResult FeatureProbe::Create(void* context, VkPhysicalDevice physical, const VkDeviceCreateInfo* info, VkDevice* output) {
        auto& probe = *static_cast<FeatureProbe*>(context);
        ++probe.creates;
        for (auto* link = static_cast<const VkBaseInStructure*>(info->pNext); link; link = link->pNext) {
            if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES) {
                ++probe.timelines;
                EXPECT_TRUE(reinterpret_cast<const VkPhysicalDeviceTimelineSemaphoreFeatures*>(link)->timelineSemaphore);
            } else if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT) {
                ++probe.maintenance;
                EXPECT_TRUE(reinterpret_cast<const VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT*>(link)->swapchainMaintenance1);
            } else
                ADD_FAILURE() << "Unexpected Device feature chain entry";
        }
        for (std::uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
            probe.swapchain_extension |= std::string_view(info->ppEnabledExtensionNames[i]) == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            probe.maintenance_extension |= std::string_view(info->ppEnabledExtensionNames[i]) == VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
        }
        return vkCreateDevice(physical, info, nullptr, output);
    }
    DeviceSlot::~DeviceSlot() {
        if (constructed)
            l::destroy_device(memory);
    }
    void DeviceSlot::construct(vsl::PhysicalDevice physical, vsl::DeviceCreateInfo info, FeatureProbe& probe) {
        l::construct_device(memory, *StartupAccess::Owned(physical), std::move(info), {&probe, FeatureProbe::Create});
        constructed = true;
    }
    std::optional<vsl::VulkanAccessor> strict_instance(std::uint32_t api) {
        const auto available = vk::enumerateInstanceExtensionProperties();
        for (const auto* name : {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                                 VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME})
            if (!std::ranges::any_of(available, [&](const auto& extension) { return std::string_view(name) == extension.extensionName.data(); }))
                return {};
        if (api < VK_API_VERSION_1_1 && !std::ranges::any_of(available, [](const auto& extension) { return std::string_view(extension.extensionName.data()) == VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME; }))
            return {};
        vsl::InstanceCreateInfo info;
        info.api_version = api;
        info.validation = true;
#ifdef __APPLE__
        info.extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        return vsl::VulkanAccessor::Create(l::glfw_instance_info(std::move(info), vsl::PresentFencePolicy::REQUIRE_FENCE));
    }
    TEST(PresentFencePolicyTest, UnknownPolicyIsRejectedBeforeGLFWInitialization) {
        EXPECT_THROW(l::glfw_instance_info({}, static_cast<vsl::PresentFencePolicy>(99)), std::invalid_argument);
    }
    TEST(PresentFencePolicyTest, MissingInstanceDependenciesAreRejectedBeforeNativeCreation) {
        unsigned creates = 0;
        o::InstanceNativeCalls calls;
        calls.context = &creates;
        calls.create_instance = [](void* context, const VkInstanceCreateInfo*, VkInstance*) {
            ++*static_cast<unsigned*>(context);
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        };
        for (const auto& extensions : std::array<std::vector<std::string>, 3>{
                 {{VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME},
                  {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME},
                  {VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME}}}) {
            vsl::impl::VulkanImplAccessor instance;
            vsl::InstanceCreateInfo info;
            info.extensions = extensions;
            EXPECT_THROW(o::initialize_instance(instance, info, calls), std::invalid_argument);
            EXPECT_FALSE(instance.instance);
            EXPECT_FALSE(instance.surface_maintenance1);
        }
        EXPECT_EQ(creates, 0u);
    }
    TEST_F(PresentFenceFeaturesVulkanIntegrationTest, MissingInstanceCapabilityRejectsDeviceFeatureBeforeNativeCreation) {
        ASSERT_FALSE(instance->surface_maintenance1);
        FeatureProbe probe;
        DeviceSlot slot;
        vsl::DeviceCreateInfo info;
        info.features = MAINTENANCE;
        EXPECT_THROW(slot.construct(physical, info, probe), std::invalid_argument);
        info.features = vsl::DeviceFeature::NONE;
        info.extensions.emplace_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
        EXPECT_THROW(slot.construct(physical, info, probe), std::invalid_argument);
        EXPECT_EQ(probe.creates, 0u);
    }
    TEST_F(PresentFenceFeaturesVulkanIntegrationTest, CoreFeatures2SupportsAllFourRequestedChains) {
        auto owner = strict_instance();
        if (!owner)
            GTEST_SKIP() << "Surface maintenance Instance extensions unavailable";
        const auto selected = vsl::PhysicalDevice::Select(*owner);
        if (!vsl::has_device_features(selected.supported_features(), MAINTENANCE | TIMELINE))
            GTEST_SKIP() << "Maintenance1 and timeline features unavailable";
        EXPECT_TRUE(StartupAccess::Owned(*(owner))->surface_capabilities2);
        EXPECT_TRUE(StartupAccess::Owned(*(owner))->surface_maintenance1);
        for (const auto features : {vsl::DeviceFeature::NONE, TIMELINE, MAINTENANCE, MAINTENANCE | TIMELINE}) {
            FeatureProbe probe;
            DeviceSlot slot;
            vsl::DeviceCreateInfo info;
            info.features = features;
            slot.construct(selected, info, probe);
            EXPECT_EQ(slot.value().enabled_features, features);
            EXPECT_EQ(probe.creates, 1u);
            EXPECT_EQ(probe.timelines, vsl::has_device_features(features, TIMELINE) ? 1u : 0u);
            EXPECT_EQ(probe.maintenance, vsl::has_device_features(features, MAINTENANCE) ? 1u : 0u);
            EXPECT_EQ(probe.maintenance_extension, vsl::has_device_features(features, MAINTENANCE));
            EXPECT_EQ(probe.swapchain_extension, vsl::has_device_features(features, MAINTENANCE));
        }
    }
    TEST_F(PresentFenceFeaturesVulkanIntegrationTest, Vulkan10UsesTheEnabledKHRFeatures2Query) {
        auto owner = strict_instance(VK_API_VERSION_1_0);
        if (!owner)
            GTEST_SKIP() << "Surface maintenance Instance extensions unavailable";
        EXPECT_TRUE(StartupAccess::Owned(*(owner))->physical_device_properties2);
        const auto selected = vsl::PhysicalDevice::Select(*owner);
        if (!vsl::has_device_features(selected.supported_features(), MAINTENANCE))
            GTEST_SKIP() << "Maintenance1 feature unavailable";
        vsl::DeviceCreateInfo info;
        info.features = MAINTENANCE;
        auto created = vsl::LogicalDeviceAccessor::Create(selected, info);
        EXPECT_TRUE(vsl::has_device_features(created.enabled_features(), MAINTENANCE));
        EXPECT_TRUE(created.data_->swapchain_enabled);
    }
    TEST_F(PresentFenceFeaturesVulkanIntegrationTest, ExtensionNameDoesNotEnableItsFeature) {
        auto owner = strict_instance();
        if (!owner)
            GTEST_SKIP() << "Surface maintenance Instance extensions unavailable";
        const auto selected = vsl::PhysicalDevice::Select(*owner);
        if (!vsl::has_device_features(selected.supported_features(), MAINTENANCE))
            GTEST_SKIP() << "Maintenance1 feature unavailable";
        FeatureProbe probe;
        DeviceSlot slot;
        vsl::DeviceCreateInfo info;
        info.extensions.emplace_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
        slot.construct(selected, info, probe);
        EXPECT_TRUE(probe.maintenance_extension);
        EXPECT_TRUE(probe.swapchain_extension);
        EXPECT_EQ(probe.maintenance, 0u);
        EXPECT_EQ(slot.value().enabled_features, vsl::DeviceFeature::NONE);
    }
    TEST_F(PresentFenceFeaturesVulkanIntegrationTest, StartupPolicyEnablesOnlyTheExplicitGraphicsDevice) {
        auto owner = strict_instance();
        if (!owner)
            GTEST_SKIP() << "Surface maintenance Instance extensions unavailable";
        const auto selected = vsl::PhysicalDevice::Select(*owner);
        if (!vsl::has_device_features(selected.supported_features(), MAINTENANCE))
            GTEST_SKIP() << "Maintenance1 feature unavailable";
        auto source = window->add_plugin<vsl::Surface>(*owner);
        const std::array requests{s::StartupRequest{{1}, s::StartupKind::DEVICE}, s::StartupRequest{{2}, s::StartupKind::DEVICE}};
        const s::StartupPlan<2> plan(requests);
        const std::array<l::StartupInit, 2> inputs{
            l::GraphicsDeviceInit{selected, *source, false, vsl::PresentFencePolicy::REQUIRE_FENCE},
            l::GraphicsDeviceInit{selected, *source, false}};
        l::StartupArena arena(plan, inputs);
        EXPECT_TRUE(vsl::has_device_features(arena.device({1}).enabled_features(), MAINTENANCE));
        EXPECT_FALSE(vsl::has_device_features(arena.device({2}).enabled_features(), MAINTENANCE));
    }
} // namespace
