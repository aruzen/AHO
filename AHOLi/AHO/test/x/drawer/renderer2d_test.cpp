#include <AHO/x/drawer/renderer2d.hpp>
#include <AHO/x/command/draw2d.hpp>
#ifdef AHO_GPU_TESTS
#include "../../support/drawing2d_fixture.hpp"
#include <AHO/image_readback.hpp>
#include <AHO/drawer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <AHO/x/screenshot.hpp>
#include "../../support/live_gpu.hpp"
namespace {
    using Renderer2DVulkanIntegrationTest = aho_test::Drawing2DVulkanIntegrationTest;

    TEST_F(Renderer2DVulkanIntegrationTest, SolidShapesTransformAndSnapshotRemainIndependentOfSourceList) {
        auto manager = make_manager();
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
        List list;
        list.rectangle({0, 0, 8, 8}, Red());
        list.set_transform({.x = 2, .y = 2});
        list.rectangle({0, 0, 4, 4}, Green());
        auto phase = manager.startPhase<vsl::ComputePhase>();
        auto cmd = renderer.draw(phase, list, 8, 8);
        ASSERT_TRUE(cmd);
        EXPECT_GT(renderer.capacity(), 0u);
        list.clear();
        phase << vsl::command::RenderPassBegin(*framebuffer) << *cmd << vsl::command::RenderPassEnd{};
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
        const auto image = pixels(manager);
        EXPECT_EQ(Pixel(image, 0, 0), (std::array<unsigned, 4>{255, 0, 0, 255}));
        EXPECT_EQ(Pixel(image, 4, 4), (std::array<unsigned, 4>{0, 255, 0, 255}));
        EXPECT_EQ(Pixel(image, 7, 7), (std::array<unsigned, 4>{255, 0, 0, 255}));
    }
    TEST_F(Renderer2DVulkanIntegrationTest, RecordPreservesAlphaCompositingOrder) {
        auto manager = make_manager();
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
        List list;
        list.rectangle({0, 0, 8, 8}, aho::RGBA(0, 0, 1, 1));
        list.rectangle({0, 0, 8, 8}, aho::RGBA(1, 0, 0, 0.5f));
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer);
            ASSERT_TRUE(renderer.record(phase, list, 8, 8));
            phase << vsl::command::RenderPassEnd{};
        }
        const auto center = Pixel(pixels(manager), 4, 4);
        EXPECT_NEAR(center[0], 128, 1);
        EXPECT_EQ(center[1], 0u);
        EXPECT_NEAR(center[2], 128, 1);
        EXPECT_EQ(center[3], 255u);
    }
    TEST_F(Renderer2DVulkanIntegrationTest, PendingSnapshotsApplyBackpressureAndReleaseUnusedCommands) {
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR, 1);
        auto manager = make_manager();
        auto phase = manager.startPhase<vsl::ComputePhase>();
        List list;
        list.rectangle({0, 0, 8, 8}, Red());
        auto first = renderer.draw(phase, list, 8, 8);
        ASSERT_TRUE(first);
        const auto capacity = renderer.capacity();
        EXPECT_FALSE(renderer.draw(phase, list, 8, 8));
        {
            auto copy = *first;
            first.reset();
            EXPECT_FALSE(renderer.draw(phase, list, 8, 8));
        }
        auto second = renderer.draw(phase, list, 8, 8);
        ASSERT_TRUE(second);
        EXPECT_EQ(renderer.capacity(), capacity);
        EXPECT_THROW(renderer.draw(phase, list, 0, 8), std::invalid_argument);
        EXPECT_THROW(renderer.draw(phase, list, 8, UINT32_MAX), std::invalid_argument);
        List textured;
        textured.rectangle({0, 0, 1, 1}, Red(), {1});
        EXPECT_THROW(renderer.draw(phase, textured, 8, 8), std::invalid_argument);
    }

    TEST_F(Renderer2DVulkanIntegrationTest, CancelledAndCompletedSubmissionsMakeSlotsReusable) {
        auto manager = make_manager();
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR, 1);
        List list;
        list.rectangle({0, 0, 8, 8}, Red());
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            auto cmd = renderer.draw(phase, list, 8, 8);
            ASSERT_TRUE(cmd);
            phase << vsl::command::RenderPassBegin(*framebuffer) << *cmd << vsl::command::RenderPassEnd{};
            phase.cancel();
        }
        list.clear();
        list.rectangle({0, 0, 8, 8}, Green());
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            auto cmd = renderer.draw(phase, list, 8, 8);
            ASSERT_TRUE(cmd);
            phase << vsl::command::RenderPassBegin(*framebuffer) << *cmd << vsl::command::RenderPassEnd{};
            EXPECT_FALSE(renderer.record(phase, list, 8, 8));
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto next = manager.startPhase<vsl::ComputePhase>();
        EXPECT_TRUE(renderer.draw(next, list, 8, 8));
        next.cancel();
        EXPECT_EQ(Pixel(pixels(manager), 4, 4), (std::array<unsigned, 4>{0, 255, 0, 255}));
    }

    TEST_F(Renderer2DVulkanIntegrationTest, UnsupportedPassIsRejectedBeforePipelineCreation) {
        EXPECT_THROW((Renderer(device, *pass, AHO_DRAW_SHADER_DIR, 1, 1)), std::out_of_range);
        vsl::RenderPassDescription info;
        info.addColor({.format = vsl::data_format::UnsignedInt32R, .finalLayout = vsl::ImageLayout::General});
        vsl::RenderPass integer_pass(device, info);
        EXPECT_THROW((Renderer(device, integer_pass, AHO_DRAW_SHADER_DIR)), std::invalid_argument);
    }
    TEST_F(Renderer2DVulkanIntegrationTest, EmptyListsDoNotConsumeSnapshotSlots) {
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR, 1);
        auto manager = make_manager();
        auto phase = manager.startPhase<vsl::ComputePhase>();
        const auto empty = renderer.draw(phase, List{}, 8, 8);
        ASSERT_TRUE(empty);
        EXPECT_EQ(renderer.capacity(), 0u);
        List list;
        list.rectangle({0, 0, 1, 1}, Red());
        EXPECT_TRUE(renderer.draw(phase, list, 8, 8));
    }
    TEST_F(Renderer2DVulkanIntegrationTest, TextureUvCropFlipAndTintReachExpectedPixels) {
        auto image = texture({255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255});
        vsl::Sampler sampler(device, vsl::SamplingMode::Nearest, vsl::EdgeMode::ClampToEdge);
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
        renderer.set_texture({1}, image, sampler);
        auto manager = make_manager();
        List list;
        list.rectangle({0, 0, 8, 8}, aho::RGBA(1, 1, 1, 1), {1});
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer);
            ASSERT_TRUE(renderer.record(phase, list, 8, 8));
            phase << vsl::command::RenderPassEnd{};
        }
        auto result = pixels(manager);
        EXPECT_EQ(Pixel(result, 1, 1), (std::array<unsigned, 4>{255, 0, 0, 255}));
        EXPECT_EQ(Pixel(result, 6, 1), (std::array<unsigned, 4>{0, 255, 0, 255}));
        EXPECT_EQ(Pixel(result, 1, 6), (std::array<unsigned, 4>{0, 0, 255, 255}));
        EXPECT_EQ(Pixel(result, 6, 6), (std::array<unsigned, 4>{255, 255, 255, 255}));
        list.clear();
        list.rectangle({0, 0, 8, 8}, aho::RGBA(.5f, 1, 1, 1), {1}, {1, 1, -.5f, -.5f});
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer);
            ASSERT_TRUE(renderer.record(phase, list, 8, 8));
            phase << vsl::command::RenderPassEnd{};
        }
        const auto tinted = Pixel(pixels(manager), 4, 4);
        EXPECT_NEAR(tinted[0], 128, 1);
        EXPECT_EQ(tinted[1], 255u);
        EXPECT_EQ(tinted[2], 255u);
        EXPECT_EQ(tinted[3], 255u);
    }

    TEST_F(Renderer2DVulkanIntegrationTest, CompletedSnapshotsReuseBuffersAcrossMultipleFrames) {
        auto manager = make_manager();
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR, 1);
        std::size_t capacity = 0;
        for (int frame = 0; frame < 6; ++frame) {
            List list;
            list.rectangle({0, 0, 8, 8}, frame % 2 ? Green() : Red());
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                auto cmd = renderer.draw(phase, list, 8, 8);
                ASSERT_TRUE(cmd);
                phase << vsl::command::RenderPassBegin(*framebuffer) << *cmd << vsl::command::RenderPassEnd{};
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
                ASSERT_TRUE(phase.completion().wait());
            }
            if (frame == 0)
                capacity = renderer.capacity();
            EXPECT_EQ(renderer.capacity(), capacity);
            const auto value = Pixel(pixels(manager), 4, 4);
            EXPECT_EQ(value, (frame % 2 ? std::array<unsigned, 4>{0, 255, 0, 255} : std::array<unsigned, 4>{255, 0, 0, 255}));
        }
    }

    TEST_F(Renderer2DVulkanIntegrationTest, TextureRegistrationRejectsInvalidResourcesAndReservedIds) {
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
        auto image = texture({255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255});
        vsl::Sampler sampler(device, vsl::SamplingMode::Nearest, vsl::EdgeMode::ClampToEdge);
        EXPECT_THROW(renderer.set_texture({0}, image, sampler), std::invalid_argument);
        EXPECT_THROW(renderer.set_texture({1}, {}, sampler), std::invalid_argument);
        EXPECT_THROW(renderer.set_texture({1}, image, {}), std::invalid_argument);
        EXPECT_THROW(renderer.set_texture({1}, *color, sampler), std::invalid_argument);
        renderer.set_texture({1}, image, sampler);
        auto copy = renderer;
        EXPECT_TRUE(copy.erase_texture({1}));
        EXPECT_FALSE(renderer.erase_texture({1}));
    }

    using Renderer2DWindowIntegrationTest = aho_test::LiveGpuTest;

    TEST_F(Renderer2DWindowIntegrationTest, DrawPhaseAndScreenshotUseThePublicRenderer) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->getWindow();
        aho::x::Screenshot screenshot(window);
        const auto size = window.frame_size();
        const auto width = static_cast<std::uint32_t>(size.value.___AN1);
        const auto height = static_cast<std::uint32_t>(size.value.___AN2);
        aho::x::Renderer2D renderer(engine->_data->logical_device, window._data2->render_pass, AHO_DRAW_SHADER_DIR);
        aho::x::DrawList list;
        list.rectangle({0, 0, static_cast<float>(width), static_cast<float>(height)}, aho::RGBA(0, 1, 0, 1));
        {
            aho::DrawPhase phase(*engine);
            ASSERT_EQ(phase.state, vsl::ComputePhase::State::WaitSubmit);
            ASSERT_TRUE(renderer.record(phase, list, width, height));
            ASSERT_TRUE(screenshot.capture(phase));
        }
        const auto image = screenshot.read();
        EXPECT_EQ(image.width, width);
        EXPECT_EQ(image.height, height);
        const auto center = (image.height / 2) * image.rowStride + (image.width / 2) * 4;
        EXPECT_EQ(std::to_integer<unsigned>(image.bytes[center]), 0u);
        EXPECT_EQ(std::to_integer<unsigned>(image.bytes[center + 1]), 255u);
        EXPECT_EQ(std::to_integer<unsigned>(image.bytes[center + 2]), 0u);
        EXPECT_EQ(std::to_integer<unsigned>(image.bytes[center + 3]), 255u);
    }
} // namespace
#else
#include <gtest/gtest.h>
TEST(Renderer2DTest, InvalidConstructionFailsBeforeNativeDeviceAccess) {
    EXPECT_THROW((aho::x::Renderer2D({}, {}, "", 0)), std::invalid_argument);
}
#endif
