#include "support/render_fixture.hpp"
namespace {
    using DrawVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST(DrawTest, ConstructorsAndManipulatorKeepVertexCount) {
        EXPECT_EQ(vsl::command::Draw{}.size, 0u);
        EXPECT_EQ(vsl::command::Draw{3}.size, 3u);
        vsl::command::DrawManip draw;
        draw.setVertexSize(3);
        ASSERT_TRUE(draw.size.has_value());
        EXPECT_EQ(*draw.size, 3u);
        draw.setVertexSize(std::nullopt);
        EXPECT_FALSE(draw.size.has_value());
    }
    TEST_F(DrawVulkanIntegrationTest, DrawsTriangleWithKnownPixelColor) {
        expect_triangle_color(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(DrawVulkanIntegrationTest, SharedManipulatorDrawsRequestedVertexCount) {
        auto draw = std::make_shared<vsl::command::DrawManip>();
        draw->setVertexSize(3);
        expect_triangle_color(render([&](auto& stream) {
            stream.vertexSize = 3;
            stream << std::static_pointer_cast<vsl::command::__Command>(draw);
        }));
    }
} // namespace
