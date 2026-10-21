#include "support/vulkan_fixture.hpp"

namespace {
    using DepthStencilVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(DepthStencilVulkanIntegrationTest, ProvidesDepthStencilStateWhenInjected) {
        auto layout = make_layout_state();
        vsl::pipeline_layout::DepthStencil{}.injection(layout);
        EXPECT_TRUE(layout.data_->info->depth_stencil.has_value());
    }
} // namespace

namespace {
    TEST_F(DepthStencilVulkanIntegrationTest, PreservesDepthAndFrontBackStencilSettings) {
        namespace pl = vsl::pipeline_layout;
        auto layout = make_layout_state();
        pl::DepthStencil settings{.depth_test = false, .depth_write = false, .depth_compare = pl::CompareOp::GREATER, .stencil_test = true, .front = {.pass = pl::StencilOp::REPLACE, .reference = 7}, .back = {.fail = pl::StencilOp::ZERO, .write_mask = 15}};
        settings.injection(layout);
        const auto& state = layout.data_->info->depth_stencil.value();
        EXPECT_FALSE(state.depthTestEnable);
        EXPECT_FALSE(state.depthWriteEnable);
        EXPECT_EQ(state.depthCompareOp, vk::CompareOp::eGreater);
        EXPECT_TRUE(state.stencilTestEnable);
        EXPECT_EQ(state.front.passOp, vk::StencilOp::eReplace);
        EXPECT_EQ(state.front.reference, 7u);
        EXPECT_EQ(state.back.failOp, vk::StencilOp::eZero);
        EXPECT_EQ(state.back.writeMask, 15u);
    }
} // namespace
