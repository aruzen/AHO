#include <VSL/lifetime/pipeline_recipe_access.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::PipelineRecipeAccess;
}

namespace {
    namespace pl = vsl::pipeline_layout;
    using InputAssemblyVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(InputAssemblyVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::InputAssembly{}.injection(layout);
        EXPECT_EQ(PipelineRecipeAccess::Read(layout).graphics_state.input_assembly.topology, vk::PrimitiveTopology::eTriangleList);
        EXPECT_FALSE(PipelineRecipeAccess::Read(layout).graphics_state.input_assembly.primitiveRestartEnable);
    }
} // namespace
