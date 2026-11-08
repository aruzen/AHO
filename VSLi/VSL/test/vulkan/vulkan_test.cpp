#include "support/vulkan_fixture.hpp"
namespace {
    using VulkanInstanceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(VulkanInstanceVulkanIntegrationTest, AccessorSharesInstanceAndEnumeratesPhysicalDevices) {
        vsl::VulkanAccessor first{instance}, second = first;
        EXPECT_EQ(first.accessor, second.accessor);
        EXPECT_TRUE(static_cast<bool>(first.accessor->instance));
        EXPECT_FALSE(first.accessor->instance.enumeratePhysicalDevices().empty());
    }
} // namespace

#include "support/window_fixture.hpp"
#ifndef _WIN32
#include <fcntl.h>
#endif
namespace {
    using VulkanConstructorWindowIntegrationTest = vsl_test::WindowIntegrationTest;
    TEST_F(VulkanConstructorWindowIntegrationTest, ReleaseFacadeUsesTheSameRuntimeWithoutClosingHostDescriptors) {
        std::vector<const char*> required;
        for (const auto& extension : vk::enumerateInstanceExtensionProperties())
            if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
                required.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#ifndef _WIN32
        const std::array<int, 3> flags{fcntl(0, F_GETFD), fcntl(1, F_GETFD), fcntl(2, F_GETFD)};
#endif
        {
            vsl::Vulkan<false> vulkan("release host streams", required);
            EXPECT_EQ(vulkan.data_, vulkan.accessor);
            EXPECT_FALSE(vulkan.data_->debug_messenger);
            EXPECT_TRUE(vulkan.owner_ref().managed());
            EXPECT_FALSE(vulkan.devices().get().empty());
        }
#ifndef _WIN32
        for (int descriptor = 0; descriptor < 3; ++descriptor)
            EXPECT_EQ(fcntl(descriptor, F_GETFD), flags[descriptor]);
#endif
    }
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
