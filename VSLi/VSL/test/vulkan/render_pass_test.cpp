#include "support/render_fixture.hpp"
namespace {
using RenderPassVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
TEST_F(RenderPassVulkanIntegrationTest, CreatesPublicRenderPassAndPickingImagesAtRequestedExtent) {
    vsl::RenderPass normal(description);
    EXPECT_TRUE(static_cast<bool>(normal._data->renderPass));
    EXPECT_EQ(normal._data->device, device._data);
    vsl::IDPickingRenderPass picking(description);
    ASSERT_EQ(picking.picking_buffer.size(), 1u);
    EXPECT_EQ(picking.picking_buffer.front()._data->width, 8u);
    EXPECT_EQ(picking.picking_buffer.front()._data->height, 8u);
    EXPECT_EQ(picking.picking_buffer.front().size(), 8u * 8u * 4u);
    picking.recreate_buffer(12, 6);
    EXPECT_EQ(picking.picking_buffer.front()._data->width, 12u);
    EXPECT_EQ(picking.picking_buffer.front()._data->height, 6u);
    EXPECT_EQ(picking.picking_buffer.front().size(), 12u * 6u * 4u);
}
TEST_F(RenderPassVulkanIntegrationTest, DISABLED_PickingPassProvidesInitializedBaseAccessor) {
    vsl::IDPickingRenderPass picking(description);
    const vsl::RenderPassAccessor base = picking;
    EXPECT_EQ(base._data, picking._data); // I04: derived _data shadows base.
}
} // namespace
