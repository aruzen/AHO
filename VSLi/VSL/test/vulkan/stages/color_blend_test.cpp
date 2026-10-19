#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using ColorBlendVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(ColorBlendVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::ColorBlend{}.injection(layout);
        const auto& state = layout._data->info->colorBlend;
        ASSERT_EQ(state.attachmentCount, 1u);
        ASSERT_NE(state.pAttachments, nullptr);
        const auto& attachment = state.pAttachments[0];
        EXPECT_TRUE(attachment.blendEnable);
        EXPECT_EQ(attachment.colorWriteMask, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                 vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        EXPECT_EQ(attachment.srcColorBlendFactor, vk::BlendFactor::eSrcAlpha);
        EXPECT_EQ(attachment.dstColorBlendFactor, vk::BlendFactor::eOneMinusSrcAlpha);
        EXPECT_EQ(attachment.colorBlendOp, vk::BlendOp::eAdd);
        EXPECT_FALSE(state.logicOpEnable);
        pl::ColorBlend{}.injection(layout);
        EXPECT_EQ(layout._data->info->colorBlend.attachmentCount, 2u);
    }
} // namespace
