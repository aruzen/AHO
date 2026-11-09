#include <VSL/lifetime/pipeline_recipe_access.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::PipelineRecipeAccess;
}

namespace {
    namespace pl = vsl::pipeline_layout;
    using MultisampleVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(MultisampleVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::Multisample{}.injection(layout);
        const auto& state = PipelineRecipeAccess::Read(layout).graphics_state.multisample;
        EXPECT_EQ(state.rasterizationSamples, vk::SampleCountFlagBits::e1);
        EXPECT_FALSE(state.sampleShadingEnable);
        EXPECT_FALSE(state.alphaToCoverageEnable);
        EXPECT_FALSE(state.alphaToOneEnable);
    }
} // namespace
