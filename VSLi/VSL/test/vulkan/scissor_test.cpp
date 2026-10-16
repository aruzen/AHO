#include "support/render_fixture.hpp"
namespace {
TEST(ScissorTest, ConstructorsPreserveRectangleCoordinates) {
    vsl::Scissor defaults;
    EXPECT_EQ(defaults.x, 0);
    EXPECT_EQ(defaults.y, 0);
    EXPECT_EQ(defaults.width, 1u);
    EXPECT_EQ(defaults.height, 1u);
    float x = 2, y = 3, width = 4, height = 5;
    vsl::__VSLD2RectangleAccessor<float> rect{{x, y}, {width, height}};
    vsl::Scissor direct(x, y, width, height), adapted(rect);
    EXPECT_EQ(direct.x, 2);
    EXPECT_EQ(direct.y, 3);
    EXPECT_EQ(direct.width, 4u);
    EXPECT_EQ(direct.height, 5u);
    EXPECT_EQ(adapted.x, 2);
    EXPECT_EQ(adapted.y, 3);
    EXPECT_EQ(adapted.width, 4u);
    EXPECT_EQ(adapted.height, 5u);
}
using ScissorVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
TEST_F(ScissorVulkanIntegrationTest, ScissorExcludesCenterPixel) {
    vsl::Scissor scissor(0, 0, 1, 1);
    auto state = make_layout_state();
    scissor.injection(state);
    ASSERT_EQ(state._data->info->scissors.size(), 1u);
    EXPECT_EQ(state._data->info->scissors[0].extent, (vk::Extent2D{1, 1}));
    vsl::Scissor all(description);
    EXPECT_EQ(all.width, 8u);
    EXPECT_EQ(all.height, 8u);
    EXPECT_EQ(render([&](auto &stream) { stream << scissor << vsl::command::Draw{3}; }),
              (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
}
} // namespace
