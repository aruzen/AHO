#include "support/render_fixture.hpp"
namespace {
using FrameBufferVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
TEST_F(FrameBufferVulkanIntegrationTest, CreatesFramebufferForEveryImageAndRendersToIt) {
    ASSERT_EQ(framebuffer->_data->swapChainFramebuffers.size(), 1u);
    EXPECT_TRUE(static_cast<bool>(framebuffer->_data->swapChainFramebuffers.front()));
    expect_triangle_color(render([](auto &stream) { stream << vsl::command::Draw{3}; }));
}
} // namespace
namespace {
TEST_F(FrameBufferVulkanIntegrationTest, CreatesPickingFramebufferWithBothAttachments) {
    vsl::IDPickingRenderPass picking(description);
    vsl::FrameBuffer picked(description, picking);
    ASSERT_EQ(picked._data->swapChainFramebuffers.size(), 1u);
    EXPECT_TRUE(static_cast<bool>(picked._data->swapChainFramebuffers.front()));
}
} // namespace
