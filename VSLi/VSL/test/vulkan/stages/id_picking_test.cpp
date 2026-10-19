#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using IdPickingVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(IdPickingVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::ColorBlend{}.injection(layout);
        pl::IDPicking{}.injection(layout);
        const auto &state = layout._data->info->colorBlend;
        ASSERT_EQ(state.attachmentCount, 2u);
        EXPECT_TRUE(state.pAttachments[0].blendEnable);
        EXPECT_FALSE(state.pAttachments[1].blendEnable);
        EXPECT_EQ(state.pAttachments[1].colorWriteMask, vk::ColorComponentFlagBits::eR);
    }
} // namespace
