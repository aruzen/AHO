#include "support/vulkan_fixture.hpp"

namespace {
using DepthStencilVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(DepthStencilVulkanIntegrationTest, ProvidesDepthStencilStateWhenInjected) {
    auto layout = make_layout_state();
    vsl::pipeline_layout::DepthStencil{}.injection(layout);
    EXPECT_TRUE(layout._data->info->depthStencil.has_value());
}
}

namespace {
TEST_F(DepthStencilVulkanIntegrationTest, PreservesDepthAndFrontBackStencilSettings) {
    namespace pl=vsl::pipeline_layout;
    auto layout=make_layout_state();
    pl::DepthStencil settings{.depthTest=false,.depthWrite=false,.depthCompare=pl::CompareOp::Greater,
        .stencilTest=true,.front={.pass=pl::StencilOp::Replace,.reference=7},
        .back={.fail=pl::StencilOp::Zero,.writeMask=15}};
    settings.injection(layout);
    const auto &state=layout._data->info->depthStencil.value();
    EXPECT_FALSE(state.depthTestEnable); EXPECT_FALSE(state.depthWriteEnable);
    EXPECT_EQ(state.depthCompareOp,vk::CompareOp::eGreater); EXPECT_TRUE(state.stencilTestEnable);
    EXPECT_EQ(state.front.passOp,vk::StencilOp::eReplace); EXPECT_EQ(state.front.reference,7u);
    EXPECT_EQ(state.back.failOp,vk::StencilOp::eZero); EXPECT_EQ(state.back.writeMask,15u);
}
}
