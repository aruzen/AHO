#include <AHO/render_context.hpp>
#ifdef AHO_GPU_TESTS
#include "support/live_gpu.hpp"
#include <AHO/drawer/draw_phase.hpp>

namespace {
    using RenderContextVulkanIntegration = aho_test::LiveGPUTest;

    TEST_F(RenderContextVulkanIntegration, WindowAndPhaseExposeTheSameDrawingServices) {
        ASSERT_NO_THROW(start_engine());
        const auto context = engine->get_window().render_context();
        EXPECT_EQ(context.device().data_, engine->data()->logical_device.data_);
        EXPECT_EQ(context.commands().data_, engine->data()->command_manager.data_);
        EXPECT_EQ(&context.descriptor_manager(), engine->data()->graphic_resource_manager.get());
        EXPECT_EQ(context.frame_count(), engine->data()->command_manager.get_buffer().get_size());
        aho::DrawPhase phase(*engine);
        const auto drawing = phase.context();
        EXPECT_EQ(drawing.device().data_, context.device().data_);
        EXPECT_EQ(drawing.render_pass().data_, context.render_pass().data_);
        EXPECT_EQ(drawing.scissor().width, context.scissor().width);
        EXPECT_EQ(drawing.scissor().height, context.scissor().height);
    }

    TEST_F(RenderContextVulkanIntegration, RetainedContextFollowsResizeAndPreservesCompatiblePass) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->get_window();
        const auto context = window.render_context();
        const auto old_format = context.format();
        const auto old_pass = context.render_pass();
        const auto old_size = context.scissor();
        ASSERT_TRUE(window.resize(420, 320));
        const auto current = window.render_context();
        EXPECT_EQ(context.render_pass().data_, current.render_pass().data_);
        EXPECT_EQ(context.scissor().width, current.scissor().width);
        EXPECT_EQ(context.scissor().height, current.scissor().height);
        EXPECT_EQ(context.viewport().width, float(current.scissor().width));
        EXPECT_EQ(context.viewport().height, float(current.scissor().height));
        EXPECT_TRUE(context.scissor().width != old_size.width || context.scissor().height != old_size.height);
        if (context.format() == old_format)
            EXPECT_EQ(context.render_pass().data_, old_pass.data_);
    }

    TEST_F(RenderContextVulkanIntegration, ClosedWindowInvalidatesRetainedContextsAndRejectsNewOnes) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->get_window();
        const auto context = window.render_context();
        ASSERT_TRUE(window.close());
        EXPECT_THROW(window.render_context(), std::logic_error);
        EXPECT_THROW(context.device(), std::logic_error);
        EXPECT_THROW(context.descriptor_manager(), std::logic_error);
        EXPECT_THROW(context.commands(), std::logic_error);
        EXPECT_THROW(context.frame_count(), std::logic_error);
        EXPECT_THROW(context.render_pass(), std::logic_error);
        EXPECT_THROW(context.format(), std::logic_error);
        EXPECT_THROW(context.viewport(), std::logic_error);
        EXPECT_THROW(context.scissor(), std::logic_error);
    }
} // namespace
#endif
