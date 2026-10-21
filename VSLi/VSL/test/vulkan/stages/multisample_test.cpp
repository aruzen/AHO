#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using MultisampleVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(MultisampleVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::Multisample{}.injection(layout);
        const auto& state = layout.data_->info->multisample;
        EXPECT_EQ(state.rasterizationSamples, vk::SampleCountFlagBits::e1);
        EXPECT_FALSE(state.sampleShadingEnable);
        EXPECT_FALSE(state.alphaToCoverageEnable);
        EXPECT_FALSE(state.alphaToOneEnable);
    }
} // namespace
