#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using InputAssemblyVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(InputAssemblyVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::InputAssembly{}.injection(layout);
        EXPECT_EQ(layout._data->info->inputAssembly.topology, vk::PrimitiveTopology::eTriangleList);
        EXPECT_FALSE(layout._data->info->inputAssembly.primitiveRestartEnable);
    }
} // namespace
