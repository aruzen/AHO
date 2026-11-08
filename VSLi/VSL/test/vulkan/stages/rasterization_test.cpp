#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using RasterizationVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(RasterizationVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::Rasterization{}.injection(layout);
        EXPECT_EQ(layout.recipe().graphics_state.rasterization.cullMode, vk::CullModeFlagBits::eBack);
        EXPECT_EQ(layout.recipe().graphics_state.rasterization.frontFace, vk::FrontFace::eCounterClockwise);
        for (const auto mode : {pl::CullingMode::NONE, pl::CullingMode::FRONT, pl::CullingMode::BACK,
                                pl::CullingMode::FRONT_AND_BACK}) {
            pl::Rasterization{mode, pl::FrontFace::CLOCKWISE}.injection(layout);
            EXPECT_EQ(layout.recipe().graphics_state.rasterization.cullMode,
                      vk::CullModeFlags(static_cast<vk::CullModeFlagBits>(mode)));
            EXPECT_EQ(layout.recipe().graphics_state.rasterization.frontFace, vk::FrontFace::eClockwise);
            EXPECT_EQ(layout.recipe().graphics_state.rasterization.polygonMode, vk::PolygonMode::eFill);
            EXPECT_FLOAT_EQ(layout.recipe().graphics_state.rasterization.lineWidth, 1.0f);
        }
    }
} // namespace
