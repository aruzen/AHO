#include "support/compute_fixture.hpp"
namespace {
TEST(DispatchTest, KeepsAllThreeWorkgroupCounts) {
    vsl::command::Dispatch dispatch(2, 3, 4);
    EXPECT_EQ(dispatch.x, 2u);
    EXPECT_EQ(dispatch.y, 3u);
    EXPECT_EQ(dispatch.z, 4u);
}
using DispatchVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
TEST_F(DispatchVulkanIntegrationTest, WritesEveryStorageBufferElementOnGpu) {
    expect_sequence(execute(100), 100);
}
} // namespace
