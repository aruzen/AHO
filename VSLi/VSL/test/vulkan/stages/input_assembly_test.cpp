#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using InputAssemblyVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(InputAssemblyVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::InputAssembly{}.injection(layout);
        EXPECT_EQ(layout.data_->info->input_assembly.topology, vk::PrimitiveTopology::eTriangleList);
        EXPECT_FALSE(layout.data_->info->input_assembly.primitiveRestartEnable);
    }
} // namespace
