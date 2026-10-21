#include "support/compute_fixture.hpp"
namespace {
    TEST(DispatchTest, KeepsAllThreeWorkgroupCounts) {
        vsl::command::Dispatch dispatch(2, 3, 4);
        EXPECT_EQ(dispatch.x, 2u);
        EXPECT_EQ(dispatch.y, 3u);
        EXPECT_EQ(dispatch.z, 4u);
    }
    TEST(DispatchTest, DefaultsMissingDimensionsToOne) {
        vsl::command::Dispatch one_dimension(7);
        EXPECT_EQ(one_dimension.x, 7u);
        EXPECT_EQ(one_dimension.y, 1u);
        EXPECT_EQ(one_dimension.z, 1u);
        vsl::command::Dispatch two_dimensions(7, 3);
        EXPECT_EQ(two_dimensions.x, 7u);
        EXPECT_EQ(two_dimensions.y, 3u);
        EXPECT_EQ(two_dimensions.z, 1u);
    }
    using DispatchVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(DispatchVulkanIntegrationTest, WritesEveryStorageBufferElementOnGPU) {
        ExpectSequence(execute(100), 100);
    }
} // namespace
