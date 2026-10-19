#include "support/vulkan_fixture.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace {
    using VulkanInstanceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(VulkanInstanceVulkanIntegrationTest, AccessorSharesInstanceAndEnumeratesPhysicalDevices) {
        vsl::VulkanAccessor first{instance}, second = first;
        EXPECT_EQ(first._accessor, second._accessor);
        EXPECT_TRUE(static_cast<bool>(first._accessor->instance));
        EXPECT_FALSE(first._accessor->instance.enumeratePhysicalDevices().empty());
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
        EXPECT_TRUE(vulkan.checkValidationLayerSupport());
        EXPECT_TRUE(static_cast<bool>(vulkan._data->debugMessenger));
        EXPECT_EQ(vulkan._accessor, vulkan._data);
        EXPECT_FALSE(vulkan.devices().get().empty());
    }
} // namespace

namespace {
    void verify_standard_streams_survive() {
        const auto descriptor = [](FILE* stream) {
#ifdef _WIN32
            return _fileno(stream);
#else
            return fileno(stream);
#endif
        };
        const auto descriptor_is_open = [](int fd) {
#ifdef _WIN32
            const int copy = _dup(fd);
            if (copy >= 0)
                _close(copy);
#else
            const int copy = dup(fd);
            if (copy >= 0)
                close(copy);
#endif
            return copy >= 0;
        };
        const std::array descriptors{descriptor(stdin), descriptor(stdout), descriptor(stderr)};
        for (const auto fd : descriptors)
            if (!descriptor_is_open(fd))
                std::_Exit(10);
        std::vector<const char*> required;
        for (const auto& extension : vk::enumerateInstanceExtensionProperties())
            if (std::strcmp(extension.extensionName, "VK_KHR_portability_enumeration") == 0)
                required.push_back("VK_KHR_portability_enumeration");
        {
            vsl::Vulkan<false> vulkan("stdio preservation test", required);
            // Never inspect a FILE after a failed descriptor check: the old implementation closes it.
            for (const auto fd : descriptors)
                if (!descriptor_is_open(fd))
                    std::_Exit(12);
            if (!vulkan._accessor || vulkan._accessor != vulkan._data ||
                vulkan._data->instance.enumeratePhysicalDevices().empty())
                std::_Exit(11);
            if (std::fputs("vsl-stdout-survives-construction\n", stdout) < 0 || std::fflush(stdout) != 0)
                std::_Exit(13);
        }
        for (const auto fd : descriptors)
            if (!descriptor_is_open(fd))
                std::_Exit(14);
        if (std::fputs("vsl-stderr-survives-destruction\n", stderr) < 0 || std::fflush(stderr) != 0)
            std::_Exit(15);
        std::_Exit(0);
    }
    TEST_F(VulkanConstructorWindowIntegrationTest, ValidationDisabledConstructorAndDestructorPreserveStandardStreams) {
        const auto previous_style = GTEST_FLAG_GET(death_test_style);
        GTEST_FLAG_SET(death_test_style, "threadsafe");
        // Re-exec isolates the process-wide failure and avoids forking initialized graphics state.
        EXPECT_EXIT(verify_standard_streams_survive(), testing::ExitedWithCode(0), "vsl-stderr-survives-destruction");
        GTEST_FLAG_SET(death_test_style, previous_style);
    }
} // namespace
