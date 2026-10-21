#include "support/render_fixture.hpp"
namespace {
    using FrameBufferVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(FrameBufferVulkanIntegrationTest, CreatesFramebufferForEveryImageAndRendersToIt) {
        ASSERT_EQ(framebuffer->data_->swap_chain_framebuffers.size(), 1u);
        EXPECT_TRUE(static_cast<bool>(framebuffer->data_->swap_chain_framebuffers.front()));
        ExpectTriangleColor(render([](auto& stream) { stream << VSL_NAMESPACE::command::Draw{3}; }));
    }
} // namespace
namespace {
    TEST_F(FrameBufferVulkanIntegrationTest, CreatesPickingFramebufferWithBothAttachments) {
        VSL_NAMESPACE::IDPickingRenderPass picking(description);
        VSL_NAMESPACE::FrameBuffer picked(description, picking);
        ASSERT_EQ(picked.data_->swap_chain_framebuffers.size(), 1u);
        EXPECT_TRUE(static_cast<bool>(picked.data_->swap_chain_framebuffers.front()));
    }
} // namespace

namespace {
    TEST_F(FrameBufferVulkanIntegrationTest, GenericTargetRetainsImagesAndPassAndRejectsMismatches) {
        VSL_NAMESPACE::FrameBuffer target(*pass, 8, 8, {*color});
        EXPECT_EQ(target.data_->render_pass, pass->data_);
        EXPECT_EQ(target.data_->attachments[0][0], color->data_);
        EXPECT_THROW((VSL_NAMESPACE::FrameBuffer(*pass, 8, 8, {})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::FrameBuffer(*pass, 9, 8, {*color})), std::invalid_argument);
        ColorImage wrong(device, 8, 8, VSL_NAMESPACE::data_format::UNSIGNED_INT);
        EXPECT_THROW((VSL_NAMESPACE::FrameBuffer(*pass, 8, 8, {wrong})), std::invalid_argument);
        auto retained = color->data_;
        color.reset();
        EXPECT_EQ(target.data_->attachments[0][0], retained);
    }
} // namespace

namespace {
    class AttachmentBindingVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(AttachmentBindingVulkanIntegrationTest, FramebufferBindingsRejectMissingDuplicateAndInvalidIds) {
        ColorImage ids(device, WIDTH, HEIGHT, VSL_NAMESPACE::data_format::UNSIGNED_INT);
        VSL_NAMESPACE::RenderPassDescription description;
        auto color_id = description.add_color({.format = color->get_format()});
        auto object_id = description.add_color({.format = ids.get_format()});
        VSL_NAMESPACE::RenderPass pass(device, description);
        auto target = VSL_NAMESPACE::FrameBuffer::FromAttachments(pass, WIDTH, HEIGHT, {{object_id, ids}, {color_id, *color}});
        EXPECT_EQ(target.data_->attachments[0][color_id], color->data_);
        EXPECT_EQ(target.data_->attachments[0][object_id], ids.data_);
        EXPECT_THROW((VSL_NAMESPACE::FrameBuffer::FromAttachments(pass, WIDTH, HEIGHT, {{color_id, *color}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::FrameBuffer::FromAttachments(pass, WIDTH, HEIGHT, {{color_id, *color}, {color_id, *color}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::FrameBuffer::FromAttachments(pass, WIDTH, HEIGHT, {{color_id, *color}, {999, ids}})), std::out_of_range);
        EXPECT_THROW((VSL_NAMESPACE::FrameBuffer::FromAttachments(pass, WIDTH, HEIGHT, {{color_id, ids}, {object_id, *color}})), std::invalid_argument);
    }
} // namespace
