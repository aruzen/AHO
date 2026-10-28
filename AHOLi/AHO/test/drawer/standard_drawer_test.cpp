#include <AHO/drawer/standard_drawer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <gtest/gtest.h>
#include <limits>
#if defined(AHO_LINK_STANDARD_DRAWER)
int main() {
    aho::Triangle<float, vsl::D2> triangle(aho::d2::PointF(0, 0), aho::d2::PointF(1, 0), aho::d2::PointF(0, 1));
    aho::draw(triangle, aho::RGBA(1, 0, 0, 1));
}
#elif defined(AHO_CONTRACT_STANDARD_DRAWER_TUPLE)
void verify() {
    aho::Triangle<float, vsl::D2> triangle;
    aho::RGBA color(1, 0, 0, 1);
    aho::DrawStream<int> stream;
    stream << aho::pack(triangle, color);
}
#else
namespace {
    using Triangle = aho::Triangle<float, vsl::D2>;
    using ImplicitDraw = void (*)(const Triangle&, const aho::RGBA&);
    using PhaseDraw = void (*)(const Triangle&, const aho::RGBA&, aho::DrawPhase&);
    using PointerDraw = void (*)(const Triangle&, const aho::RGBA&, aho::DrawPhase*);
    static_assert(std::same_as<decltype(static_cast<ImplicitDraw>(&aho::draw)), ImplicitDraw>);
    static_assert(std::same_as<decltype(static_cast<PhaseDraw>(&aho::draw)), PhaseDraw>);
    static_assert(std::same_as<decltype(static_cast<PointerDraw>(&aho::draw)), PointerDraw>);
#ifndef AHO_GPU_TESTS
    TEST(StandardDrawerTest, MissingPhaseIsRejectedWithoutAnEngine) {
        const Triangle triangle({-1, -1}, {1, -1}, {0, 1});
        const aho::RGBA color(1, 0, 0, 1);
        ASSERT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
        EXPECT_THROW(aho::draw(triangle, color), std::logic_error);
        EXPECT_THROW(aho::draw(triangle, color, nullptr), std::logic_error);
    }
#endif
} // namespace
#endif

#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include <AHO/image_readback.hpp>

namespace {
    using StandardDrawerVulkanIntegration = aho_test::LiveGPUTest;
    Triangle triangle_at(float x);
    aho::ImagePixels capture_triangle(aho::StandardEngine& engine, const std::function<void(aho::DrawPhase&)>& record);
    std::array<unsigned, 4> pixel_at(const aho::ImagePixels& pixels, float x, float y);

    Triangle triangle_at(float x) {
        return Triangle({x - .25f, -.7f}, {x + .25f, -.7f}, {x, .7f});
    }

    aho::ImagePixels capture_triangle(aho::StandardEngine& engine, const std::function<void(aho::DrawPhase&)>& record) {
        auto window = engine.get_window();
        aho::ImageReadback readback(window.render_context().commands());
        std::optional<aho::ImageReadbackTicket> ticket;
        {
            aho::DrawPhase phase(engine, nullptr, [&](auto& stream) {
                stream << vsl::command::RenderPassEnd{};
                const auto chain = window.data2->swapchain;
                ticket = readback.record(phase, chain.images.at(phase.get_image_index()), vsl::ImageLayout::PRESENT);
            });
            record(phase);
        }
        if (!ticket)
            throw std::runtime_error("Triangle readback was not recorded");
        return ticket->read();
    }

    std::array<unsigned, 4> pixel_at(const aho::ImagePixels& pixels, float x, float y) {
        const auto column = std::uint32_t(x * pixels.width), row = std::uint32_t(y * pixels.height);
        const auto offset = std::size_t(row) * pixels.row_stride + column * 4;
        const auto format = pixels.format.type();
        const bool bgra = format == vsl::data_format::FormatTypes::SRGB8_BGRA || format == vsl::data_format::FormatTypes::UNSIGNED_NORMALIZED8_BGRA;
        return {std::to_integer<unsigned>(pixels.bytes[offset + (bgra ? 2 : 0)]),
                std::to_integer<unsigned>(pixels.bytes[offset + 1]),
                std::to_integer<unsigned>(pixels.bytes[offset + (bgra ? 0 : 2)]),
                std::to_integer<unsigned>(pixels.bytes[offset + 3])};
    }

    TEST_F(StandardDrawerVulkanIntegration, ImplicitExplicitAndTupleDrawSnapshotInputsAndReusePipeline) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->get_window();
        ASSERT_TRUE(window.recreate_swapchain(vsl::ImageType::TRANSFER_SRC));
        const auto pixels = capture_triangle(*engine, [&](auto& phase) {
            auto triangle = triangle_at(-.6f);
            aho::RGBA color(1, 0, 0, 1);
            aho::draw(triangle, color);
            const auto state = window.data2->standard_draw;
            triangle = triangle_at(0);
            color = aho::RGBA(0, 1, 0, 1);
            aho::draw(triangle, color, &phase);
            auto right = triangle_at(.6f);
            std::swap(right.pos1, right.pos2);
            phase << aho::pack(right, aho::RGBA(0, 0, 1, 1));
            EXPECT_EQ(state, window.data2->standard_draw);
        });
        EXPECT_EQ(pixel_at(pixels, .2f, .5f), (std::array<unsigned, 4>{255, 0, 0, 255}));
        EXPECT_EQ(pixel_at(pixels, .5f, .5f), (std::array<unsigned, 4>{0, 255, 0, 255}));
        EXPECT_EQ(pixel_at(pixels, .8f, .5f), (std::array<unsigned, 4>{0, 0, 255, 255}));
        const auto state = window.data2->standard_draw;
        const auto again = capture_triangle(*engine, [&](auto& phase) { aho::draw(triangle_at(0), aho::RGBA(1, 0, 0, 1), phase); });
        EXPECT_EQ(state, window.data2->standard_draw);
        EXPECT_EQ(pixel_at(again, .5f, .5f), (std::array<unsigned, 4>{255, 0, 0, 255}));
    }

    TEST_F(StandardDrawerVulkanIntegration, DrawOrderUsesStraightAlphaAndPreservesDestinationAlpha) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_TRUE(engine->get_window().recreate_swapchain(vsl::ImageType::TRANSFER_SRC));
        const auto pixels = capture_triangle(*engine, [&](auto& phase) {
            const Triangle full({-1, -1}, {3, -1}, {-1, 3});
            aho::draw(full, aho::RGBA(1, 0, 0, 1), phase);
            aho::draw(full, aho::RGBA(0, 0, 1, .5f), phase);
        });
        const auto pixel = pixel_at(pixels, .5f, .5f);
        const auto format = pixels.format.type();
        const bool srgb = format == vsl::data_format::FormatTypes::SRGB8_BGRA || format == vsl::data_format::FormatTypes::SRGB8_RGBA;
        // 線形0.5はsRGB8で約188、UNORM8で約128。量子化誤差は2まで許容する。
        const unsigned expected = srgb ? 188 : 128;
        EXPECT_NEAR(pixel[0], expected, 2);
        EXPECT_EQ(pixel[1], 0u);
        EXPECT_NEAR(pixel[2], expected, 2);
        EXPECT_EQ(pixel[3], 255u);
    }

    TEST_F(StandardDrawerVulkanIntegration, InvalidValuesAndCompletedPhasesDoNotRecordDraws) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->get_window();
        {
            aho::DrawPhase phase(*engine);
            auto triangle = triangle_at(0);
            triangle.pos1.value.___AN1 = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(aho::draw(triangle, aho::RGBA(1, 0, 0, 1), phase), std::invalid_argument);
            EXPECT_THROW(aho::draw(triangle_at(0), aho::RGBA(1, 0, 0, std::numeric_limits<float>::infinity()), phase), std::invalid_argument);
            EXPECT_FALSE(window.data2->standard_draw);
            aho::draw(triangle_at(0), aho::RGBA(1, 0, 0, 1), phase);
            ASSERT_NE(phase.submit(), vsl::ComputePhase::SubmitResult::FAILED);
            EXPECT_THROW(aho::draw(triangle_at(0), aho::RGBA(1, 0, 0, 1), phase), std::logic_error);
        }
        engine->data_->command_manager.wait_idle();
    }

    TEST_F(StandardDrawerVulkanIntegration, ResizeRefreshesViewportAndKeepsTheWindowCache) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->get_window();
        ASSERT_TRUE(window.recreate_swapchain(vsl::ImageType::TRANSFER_SRC));
        const Triangle full({-1, -1}, {3, -1}, {-1, 3});
        (void)capture_triangle(*engine, [&](auto& phase) { aho::draw(full, aho::RGBA(1, 0, 0, 1), phase); });
        const auto state = window.data2->standard_draw;
        const auto pass = window.render_context().render_pass();
        ASSERT_TRUE(window.resize(640, 360));
        const auto pixels = capture_triangle(*engine, [&](auto& phase) { phase << aho::pack(full, aho::RGBA(0, 1, 0, 1)); });
        const auto extent = window.render_context().scissor();
        EXPECT_EQ(pixels.width, extent.width);
        EXPECT_EQ(pixels.height, extent.height);
        EXPECT_EQ(pixel_at(pixels, .95f, .95f), (std::array<unsigned, 4>{0, 255, 0, 255}));
        if (pass.data_ == window.render_context().render_pass().data_)
            EXPECT_EQ(state, window.data2->standard_draw);
    }

    TEST_F(StandardDrawerVulkanIntegration, ExplicitOuterPhaseAndTupleKeepTheirWindowDuringNestedDrawing) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->get_window();
        ASSERT_TRUE(window.recreate_swapchain(vsl::ImageType::TRANSFER_SRC));
        aho::StandardEngine other("Nested triangle draw");
        const auto pixels = capture_triangle(*engine, [&](auto& outer) {
            {
                aho::DrawPhase inner(other);
                aho::draw(triangle_at(-.6f), aho::RGBA(1, 0, 0, 1), outer);
                outer << aho::pack(triangle_at(.6f), aho::RGBA(0, 0, 1, 1));
                aho::draw(triangle_at(0), aho::RGBA(0, 1, 0, 1));
                EXPECT_NE(window.data2->standard_draw, other.get_window().data2->standard_draw);
            }
            other.data_->command_manager.wait_idle();
            other.get_window().close();
        });
        EXPECT_EQ(pixel_at(pixels, .2f, .5f), (std::array<unsigned, 4>{255, 0, 0, 255}));
        EXPECT_EQ(pixel_at(pixels, .8f, .5f), (std::array<unsigned, 4>{0, 0, 255, 255}));
    }
} // namespace
#endif
