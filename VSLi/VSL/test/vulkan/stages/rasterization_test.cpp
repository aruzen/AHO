#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using RasterizationVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(RasterizationVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::Rasterization{}.injection(layout);
        EXPECT_EQ(layout._data->info->rasterization.cullMode, vk::CullModeFlagBits::eBack);
        EXPECT_EQ(layout._data->info->rasterization.frontFace, vk::FrontFace::eCounterClockwise);
        for (const auto mode : {pl::CullingMode::None, pl::CullingMode::Front, pl::CullingMode::Back,
                                pl::CullingMode::FrontAndBack}) {
            pl::Rasterization{mode, pl::FrontFace::Clockwise}.injection(layout);
            EXPECT_EQ(layout._data->info->rasterization.cullMode,
                      vk::CullModeFlags(static_cast<vk::CullModeFlagBits>(mode)));
            EXPECT_EQ(layout._data->info->rasterization.frontFace, vk::FrontFace::eClockwise);
            EXPECT_EQ(layout._data->info->rasterization.polygonMode, vk::PolygonMode::eFill);
            EXPECT_FLOAT_EQ(layout._data->info->rasterization.lineWidth, 1.0f);
        }
    }
} // namespace
