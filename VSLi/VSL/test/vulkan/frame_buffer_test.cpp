#include "support/render_fixture.hpp"
namespace {
    using FrameBufferVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(FrameBufferVulkanIntegrationTest, CreatesFramebufferForEveryImageAndRendersToIt) {
        ASSERT_EQ(framebuffer->_data->swapChainFramebuffers.size(), 1u);
        EXPECT_TRUE(static_cast<bool>(framebuffer->_data->swapChainFramebuffers.front()));
        expect_triangle_color(render([](auto &stream) { stream << vsl::command::Draw{3}; }));
    }
}
namespace {
    TEST_F(FrameBufferVulkanIntegrationTest, CreatesPickingFramebufferWithBothAttachments) {
        vsl::IDPickingRenderPass picking(description);
        vsl::FrameBuffer picked(description, picking);
        ASSERT_EQ(picked._data->swapChainFramebuffers.size(), 1u);
        EXPECT_TRUE(static_cast<bool>(picked._data->swapChainFramebuffers.front()));
    }
}

namespace {
    TEST_F(FrameBufferVulkanIntegrationTest, GenericTargetRetainsImagesAndPassAndRejectsMismatches) {
        vsl::FrameBuffer target(*pass, 8, 8, {*color});
        EXPECT_EQ(target._data->renderPass, pass->_data);
        EXPECT_EQ(target._data->attachments[0][0], color->_data);
        EXPECT_THROW((vsl::FrameBuffer(*pass, 8, 8, {})), std::invalid_argument);
        EXPECT_THROW((vsl::FrameBuffer(*pass, 9, 8, {*color})), std::invalid_argument);
        ColorImage wrong(device, 8, 8, vsl::data_format::UnsignedInt);
        EXPECT_THROW((vsl::FrameBuffer(*pass, 8, 8, {wrong})), std::invalid_argument);
        auto retained = color->_data;
        color.reset();
        EXPECT_EQ(target._data->attachments[0][0], retained);
    }
}

namespace {
    class AttachmentBindingVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(AttachmentBindingVulkanIntegrationTest, FramebufferBindingsRejectMissingDuplicateAndInvalidIds) {
        ColorImage ids(device, width, height, vsl::data_format::UnsignedInt);
        vsl::RenderPassDescription description;
        auto colorId = description.addColor({.format = color->getFormat()});
        auto objectId = description.addColor({.format = ids.getFormat()});
        vsl::RenderPass pass(device, description);
        auto target = vsl::FrameBuffer::fromAttachments(pass, width, height, {{objectId, ids}, {colorId, *color}});
        EXPECT_EQ(target._data->attachments[0][colorId], color->_data);
        EXPECT_EQ(target._data->attachments[0][objectId], ids._data);
        EXPECT_THROW((vsl::FrameBuffer::fromAttachments(pass, width, height, {{colorId, *color}})), std::invalid_argument);
        EXPECT_THROW((vsl::FrameBuffer::fromAttachments(pass, width, height, {{colorId, *color}, {colorId, *color}})), std::invalid_argument);
        EXPECT_THROW((vsl::FrameBuffer::fromAttachments(pass, width, height, {{colorId, *color}, {999, ids}})), std::out_of_range);
        EXPECT_THROW((vsl::FrameBuffer::fromAttachments(pass, width, height, {{colorId, ids}, {objectId, *color}})), std::invalid_argument);
    }
}
