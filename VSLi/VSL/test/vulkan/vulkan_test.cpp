#include "support/vulkan_fixture.hpp"
namespace {
    using VulkanInstanceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(VulkanInstanceVulkanIntegrationTest, AccessorSharesInstanceAndEnumeratesPhysicalDevices) {
        vsl::VulkanAccessor first{instance}, second = first;
        EXPECT_EQ(first.accessor, second.accessor);
        EXPECT_TRUE(static_cast<bool>(first.accessor->instance));
        EXPECT_FALSE(first.accessor->instance.enumeratePhysicalDevices().empty());
        // Public Vulkan<false> still closes stdio; validation-enabled construction is tested below.
    }
} // namespace

#include "support/window_fixture.hpp"
namespace {
    using VulkanConstructorWindowIntegrationTest = vsl_test::WindowIntegrationTest;
    TEST_F(VulkanConstructorWindowIntegrationTest, ValidationEnabledConstructorCreatesADebugMessengerWithDefaultAllocator) {
        const auto extensions = vk::enumerateInstanceExtensionProperties();
        const auto has_extension = [&](const char* name) {
            return std::ranges::any_of(extensions, [&](const auto& entry) {
                return std::strcmp(entry.extensionName, name) == 0;
            });
        };
        if (!has_extension("VK_EXT_debug_utils"))
            GTEST_SKIP() << "VK_EXT_debug_utils is unavailable";
        const auto layers = vk::enumerateInstanceLayerProperties();
        if (!std::ranges::any_of(layers, [](const auto& layer) {
                return std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
            }))
            GTEST_SKIP() << "VK_LAYER_KHRONOS_validation is unavailable";
        std::vector<const char*> required;
        for (const auto* name : {"VK_KHR_portability_enumeration", "VK_KHR_get_physical_device_properties2"})
            if (has_extension(name))
                required.push_back(name);
        vsl::Vulkan<true> vulkan("debug messenger default allocator test", required);
        EXPECT_TRUE(vulkan.check_validation_layer_support());
        EXPECT_TRUE(static_cast<bool>(vulkan.data_->debug_messenger));
        EXPECT_EQ(vulkan.accessor, vulkan.data_);
        EXPECT_FALSE(vulkan.devices().get().empty());
    }
} // namespace
