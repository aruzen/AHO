#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include "support/render_fixture.hpp"
#include <VSL/lifetime/swapchain_access.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::SwapchainAccess;
}
#include "support/deferred_action.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    using RenderPassVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(RenderPassVulkanIntegrationTest, CreatesPublicRenderPassAndPickingImagesAtRequestedExtent) {
        vsl::RenderPass normal(description);
        EXPECT_TRUE(static_cast<bool>(GraphicsAccess::Owned(normal)->render_pass));
        EXPECT_EQ(GraphicsAccess::Owned(normal)->device, device.data_.get());
        vsl::IDPickingRenderPass picking(description);
        ASSERT_EQ(picking.image_count(), 1u);
        EXPECT_EQ(ImageAccess::Owned(picking.picking_image(0))->width, 8u);
        EXPECT_EQ(ImageAccess::Owned(picking.picking_image(0))->height, 8u);
        EXPECT_EQ(picking.picking_image(0).size(), 8u * 8u * 4u);
        picking.recreate_buffer(12, 6);
        EXPECT_EQ(ImageAccess::Owned(picking.picking_image(0))->width, 12u);
        EXPECT_EQ(ImageAccess::Owned(picking.picking_image(0))->height, 6u);
        EXPECT_EQ(picking.picking_image(0).size(), 12u * 6u * 4u);
    }
    TEST_F(RenderPassVulkanIntegrationTest, PickingPassProvidesInitializedBaseAccessor) {
        vsl::IDPickingRenderPass picking(description);
        const vsl::RenderPassAccessor base = picking;
        EXPECT_EQ(GraphicsAccess::Owned(base), GraphicsAccess::Owned(picking));
    }

    class PickingReadVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override;

    protected:
        void fill_images(vsl::IDPickingRenderPass& picking, vsl::CommandManager manager);
    };

    bool PickingReadVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    void PickingReadVulkanIntegrationTest::fill_images(vsl::IDPickingRenderPass& picking, vsl::CommandManager manager) {
        vsl::RenderPassDescription clear_description;
        clear_description.add_color({.format = vsl::data_format::UNSIGNED_INT,
                                     .final_layout = vsl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL});
        vsl::RenderPass clear_pass(device, clear_description);
        for (std::uint32_t i = 0; i < picking.image_count(); ++i) {
            vsl::FrameBuffer target(clear_pass, WIDTH, HEIGHT, {picking.picking_image(i)});
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target, {vsl::ClearUint{101 + i}})
                  << vsl::command::RenderPassEnd{};
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
    }
    TEST_F(PickingReadVulkanIntegrationTest, ReadsAcquiredImageRatherThanTheCommandBufferSlot) {
        description = SwapchainAccess::FromOwned(SwapchainAccess::Owned(description), std::vector<vsl::ImageAccessor>(2, *color));
        vsl::IDPickingRenderPass picking(description);
        auto manager = make_manager();
        fill_images(picking, manager);
        vsl::SynchroManager sync(device);
        auto ready = sync.create_fence("picking read ready", true);
        const auto buffers = CommandAccess::Data(manager.get_buffer());
        ASSERT_EQ(buffers->current_buffer_idx, 0u);
        // WSIの取得順に依存せず、別画像を取得したフレームの状態を再現する。
        buffers->recording_context->acquired_image_index = 1;
        EXPECT_EQ(picking.read(manager, ready, 0, 0), 102u);
        buffers->recording_context->acquired_image_index.reset();
        EXPECT_EQ(picking.read(manager, ready, WIDTH - 1, HEIGHT - 1), 101u);
    }
    TEST_F(PickingReadVulkanIntegrationTest, ReadKeepsItsSelectedImageWhenFenceCompletionDestroysMovesOrRecreatesTheFacade) {
        description = SwapchainAccess::FromOwned(SwapchainAccess::Owned(description), std::vector<vsl::ImageAccessor>(2, *color));
        for (int action = 0; action < 3; ++action) {
            SCOPED_TRACE(action);
            auto picking = std::make_unique<vsl::IDPickingRenderPass>(description);
            std::unique_ptr<vsl::IDPickingRenderPass> moved;
            auto manager = make_manager().independent(1);
            fill_images(*picking, manager);
            vsl::SynchroManager sync(device);
            auto ready = sync.create_fence("picking pending read", false);
            const auto buffers = CommandAccess::Data(manager.get_buffer());
            bool called = false;
            const vsl_test::DeferredAction callback([&] {
                called = true;
                buffers->recording_context->acquired_image_index = 0;
                if (action == 1)
                    picking->recreate_buffer(WIDTH, HEIGHT);
                else {
                    if (action == 2)
                        moved = std::make_unique<vsl::IDPickingRenderPass>(std::move(*picking));
                    picking.reset();
                }
            });
            auto owner = callback.owner();
            {
                auto phase = manager.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, ready);
                manager.get_buffer().retain_until_complete(owner);
                owner.reset();
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            }
            device.data_->device.waitIdle();
            buffers->recording_context->acquired_image_index = 1;
            EXPECT_EQ(picking->read(manager, ready, 0, 0), 102u);
            EXPECT_TRUE(called);
        }
    }
    TEST_F(PickingReadVulkanIntegrationTest, ReadRejectsAnEmptyManagerOrMissingRecordingContextBeforeWaiting) {
        vsl::IDPickingRenderPass picking(description);
        vsl::SynchroManager sync(device);
        auto ready = sync.create_fence("picking invalid manager", true);
        EXPECT_THROW(picking.read({}, ready, 0, 0), std::logic_error);
        auto manager = make_manager();
        const auto buffers = CommandAccess::Data(manager.get_buffer());
        auto saved = std::exchange(buffers->recording_context, {});
        EXPECT_THROW(picking.read(manager, ready, 0, 0), std::logic_error);
        buffers->recording_context = std::move(saved);
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
