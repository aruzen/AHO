#include "support/render_fixture.hpp"
namespace {
    using RenderPassVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(RenderPassVulkanIntegrationTest, CreatesPublicRenderPassAndPickingImagesAtRequestedExtent) {
        vsl::RenderPass normal(description);
        EXPECT_TRUE(static_cast<bool>(normal.data_->render_pass));
        EXPECT_EQ(normal.data_->device, device.data_);
        vsl::IDPickingRenderPass picking(description);
        ASSERT_EQ(picking.picking_buffer.size(), 1u);
        EXPECT_EQ(picking.picking_buffer.front().data_->width, 8u);
        EXPECT_EQ(picking.picking_buffer.front().data_->height, 8u);
        EXPECT_EQ(picking.picking_buffer.front().size(), 8u * 8u * 4u);
        picking.recreate_buffer(12, 6);
        EXPECT_EQ(picking.picking_buffer.front().data_->width, 12u);
        EXPECT_EQ(picking.picking_buffer.front().data_->height, 6u);
        EXPECT_EQ(picking.picking_buffer.front().size(), 12u * 6u * 4u);
    }
    TEST_F(RenderPassVulkanIntegrationTest, PickingPassProvidesInitializedBaseAccessor) {
        vsl::IDPickingRenderPass picking(description);
        const vsl::RenderPassAccessor base = picking;
        EXPECT_EQ(base.data_, picking.data_);
    }

    class PickingReadVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool PickingReadVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(PickingReadVulkanIntegrationTest, ReadsAcquiredImageRatherThanTheCommandBufferSlot) {
        description.images.resize(2, *color);
        vsl::IDPickingRenderPass picking(description);
        vsl::RenderPassDescription clear_description;
        clear_description.add_color({.format = vsl::data_format::UNSIGNED_INT,
                                     .final_layout = vsl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL});
        vsl::RenderPass clear_pass(device, clear_description);
        auto manager = make_manager();
        for (std::uint32_t i = 0; i < picking.picking_buffer.size(); ++i) {
            vsl::FrameBuffer target(clear_pass, WIDTH, HEIGHT, {picking.picking_buffer[i]});
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target, {vsl::ClearUint{101 + i}})
                  << vsl::command::RenderPassEnd{};
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        vsl::SynchroManager sync(device);
        auto ready = sync.create_fence("picking read ready", true);
        const auto buffers = manager.get_buffer().data_;
        ASSERT_EQ(buffers->current_buffer_idx, 0u);
        // WSIの取得順に依存せず、別画像を取得したフレームの状態を再現する。
        buffers->acquired_image_index = 1;
        EXPECT_EQ(picking.read(manager, ready, 0, 0), 102u);
        buffers->acquired_image_index.reset();
        EXPECT_EQ(picking.read(manager, ready, WIDTH - 1, HEIGHT - 1), 101u);
    }
} // namespace

namespace {
    TEST(RenderPassDescriptionTest, CompositionPreservesAttachmentIdentityAndSubpassOrder) {
        vsl::RenderPassDescription desc;
        auto color = desc.add_color({.format = vsl::data_format::UNSIGNED_NORMALIZED8_RGBA});
        auto id = desc.add_color({.format = vsl::data_format::UNSIGNED_INT});
        auto depth = desc.set_depth_stencil({.format = vsl::data_format::DEPTH32_FLOAT,
                                             .final_layout = vsl::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
        EXPECT_EQ(color, 0u);
        EXPECT_EQ(id, 1u);
        EXPECT_EQ(depth, 2u);
        ASSERT_EQ(desc.subpasses.front().colors.size(), 2u);
        EXPECT_EQ(desc.subpasses.front().colors[1].attachment, id);
        EXPECT_EQ(desc.subpasses.front().depth_stencil->attachment, depth);
        EXPECT_NO_THROW(desc.validate());
        EXPECT_THROW(desc.set_depth_stencil({.format = vsl::data_format::DEPTH32_FLOAT}), std::invalid_argument);
    }
    TEST(RenderPassDescriptionTest, RejectsInvalidReferencesFormatsAndLoadContracts) {
        vsl::RenderPassDescription desc;
        desc.add_color({.format = vsl::data_format::UNSIGNED_NORMALIZED8_RGBA});
        auto valid = desc;
        desc.subpasses[0].colors[0].attachment = 99;
        EXPECT_THROW(desc.validate(), std::out_of_range);
        desc = valid;
        desc.attachments[0].load = vsl::LoadOp::LOAD;
        EXPECT_THROW(desc.validate(), std::invalid_argument);
        desc = valid;
        desc.attachments[0].final_layout = vsl::ImageLayout::UNDEFINED;
        EXPECT_THROW(desc.validate(), std::invalid_argument);
        desc = valid;
        desc.attachments[0].format = vsl::data_format::DEPTH32_FLOAT;
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
        desc.dependencies[0].destination_stage = vsl::PipelineStage::ALL_COMMANDS;
        EXPECT_THROW(desc.validate(), std::invalid_argument);
        desc = valid;
        desc.subpasses.clear();
        EXPECT_THROW(desc.validate(), std::invalid_argument);
    }
} // namespace
