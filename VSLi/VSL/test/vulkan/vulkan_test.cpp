#include "support/vulkan_fixture.hpp"
namespace {
using VulkanInstanceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(VulkanInstanceVulkanIntegrationTest, AccessorSharesInstanceAndEnumeratesPhysicalDevices) {
    vsl::VulkanAccessor first{instance}, second = first;
    EXPECT_EQ(first._accessor, second._accessor);
    EXPECT_TRUE(static_cast<bool>(first._accessor->instance));
    EXPECT_FALSE(first._accessor->instance.enumeratePhysicalDevices().empty());
    // Public Vulkan<false> closes stdio; Vulkan<true> debug setup dereferences a null
    // allocation-callback pointer. Constructor execution is deferred (see vsl_testing_status.md).
}
} // namespace
