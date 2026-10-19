#include "support/render_fixture.hpp"
namespace {
    TEST(ViewportTest, ConstructorsPreserveRectangleCoordinates) {
        vsl::Viewport defaults;
        EXPECT_EQ(defaults.x, 0);
        EXPECT_EQ(defaults.y, 0);
        EXPECT_EQ(defaults.width, 1);
        EXPECT_EQ(defaults.height, 1);
        float x = 2, y = 3, width = 4, height = 5;
        vsl::__VSLD2RectangleAccessor<float> rect{{x, y}, {width, height}};
        vsl::Viewport direct(x, y, width, height), adapted(rect);
        EXPECT_EQ(direct.x, 2);
        EXPECT_EQ(direct.y, 3);
        EXPECT_EQ(direct.width, 4);
        EXPECT_EQ(direct.height, 5);
        EXPECT_EQ(adapted.x, 2);
        EXPECT_EQ(adapted.y, 3);
        EXPECT_EQ(adapted.width, 4);
        EXPECT_EQ(adapted.height, 5);
    }
    using ViewportVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(ViewportVulkanIntegrationTest, SwapchainInjectionAndDynamicViewportProducePixels) {
        vsl::Viewport viewport(description);
        EXPECT_EQ(viewport.width, 8);
        EXPECT_EQ(viewport.height, 8);
        auto state = make_layout_state();
        viewport.injection(state);
        ASSERT_EQ(state._data->info->viewports.size(), 1u);
        const auto &actual = state._data->info->viewports[0];
        EXPECT_EQ(actual.width, 8);
        EXPECT_EQ(actual.height, 8);
        EXPECT_EQ(actual.minDepth, 0);
        EXPECT_EQ(actual.maxDepth, 1);
        expect_triangle_color(render([&](auto &stream) { stream << viewport << vsl::command::Draw{3}; }));
    }
} // namespace
