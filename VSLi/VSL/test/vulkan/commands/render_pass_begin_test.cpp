#include "support/render_fixture.hpp"
namespace {
using RenderPassBeginVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
TEST_F(RenderPassBeginVulkanIntegrationTest, RetainsPassAndFramebufferAndClearsBlack) {
    vsl::command::RenderPassBegin begin(*pass, *framebuffer);
    EXPECT_EQ(begin.renderPass._data, pass->_data);
    EXPECT_EQ(begin.frameBuffer._data, framebuffer->_data);
    EXPECT_FALSE(begin.clear_color.has_value());
    EXPECT_EQ(render([](auto &) {}), (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
}
} // namespace
