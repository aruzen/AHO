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
TEST_F(RenderPassVulkanIntegrationTest, PickingPassProvidesInitializedBaseAccessor) {
    vsl::IDPickingRenderPass picking(description);
    const vsl::RenderPassAccessor base = picking;
    EXPECT_EQ(base._data, picking._data);
}
}

namespace {
TEST(RenderPassDescriptionTest, CompositionPreservesAttachmentIdentityAndSubpassOrder) {
    vsl::RenderPassDescription desc;
    auto color = desc.addColor({.format = vsl::data_format::UnsignedNormalized8RGBA});
    auto id = desc.addColor({.format = vsl::data_format::UnsignedInt});
    auto depth = desc.setDepthStencil({.format = vsl::data_format::Depth32Float,
                                       .finalLayout = vsl::ImageLayout::DepthStencilAttachmentOptimal});
    EXPECT_EQ(color, 0u);
    EXPECT_EQ(id, 1u);
    EXPECT_EQ(depth, 2u);
    ASSERT_EQ(desc.subpasses.front().colors.size(), 2u);
    EXPECT_EQ(desc.subpasses.front().colors[1].attachment, id);
    EXPECT_EQ(desc.subpasses.front().depthStencil->attachment, depth);
    EXPECT_NO_THROW(desc.validate());
    EXPECT_THROW(desc.setDepthStencil({.format = vsl::data_format::Depth32Float}), std::invalid_argument);
}
TEST(RenderPassDescriptionTest, RejectsInvalidReferencesFormatsAndLoadContracts) {
    vsl::RenderPassDescription desc;
    desc.addColor({.format = vsl::data_format::UnsignedNormalized8RGBA});
    auto valid = desc;
    desc.subpasses[0].colors[0].attachment = 99;
    EXPECT_THROW(desc.validate(), std::out_of_range);
    desc = valid;
    desc.attachments[0].load = vsl::LoadOp::Load;
    EXPECT_THROW(desc.validate(), std::invalid_argument);
    desc = valid;
    desc.attachments[0].finalLayout = vsl::ImageLayout::Undefined;
    EXPECT_THROW(desc.validate(), std::invalid_argument);
    desc = valid;
    desc.attachments[0].format = vsl::data_format::Depth32Float;
    EXPECT_THROW(desc.validate(), std::invalid_argument);
    desc = valid;
    desc.subpasses[0].resolves = {{0}, {0}};
    EXPECT_THROW(desc.validate(), std::invalid_argument);
    desc = valid;
    desc.dependencies[0].destination = 99;
    EXPECT_THROW(desc.validate(), std::invalid_argument);
    desc = valid;
    desc.subpasses[0].preserve = {0};
    EXPECT_THROW(desc.validate(), std::invalid_argument);
    desc = valid;
    desc.dependencies[0].destinationStage = vsl::PipelineStage::AllCommands;
    EXPECT_THROW(desc.validate(), std::invalid_argument);
    desc = valid;
    desc.subpasses.clear();
    EXPECT_THROW(desc.validate(), std::invalid_argument);
}
}
