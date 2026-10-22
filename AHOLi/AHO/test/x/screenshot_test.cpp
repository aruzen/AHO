#include <AHO/x/screenshot.hpp>
#include "../support/temp_directory.hpp"
#include <gtest/gtest.h>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include "../../../../thirdparty/stb_image.h"
#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include <AHO/drawer/draw_phase.hpp>
#endif

namespace {
    struct PngPixels {
        int width = 0, height = 0;
        std::vector<unsigned char> bytes;
    };
    PngPixels load_png(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        const std::vector<unsigned char> encoded{std::istreambuf_iterator<char>(stream), {}};
        PngPixels result;
        int channels = 0;
        std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
            stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &result.width, &result.height, &channels, 4),
            &stbi_image_free);
        if (!pixels)
            throw std::runtime_error("Could not decode saved PNG");
        result.bytes.assign(pixels.get(), pixels.get() + result.width * result.height * 4);
        return result;
    }

#ifndef AHO_GPU_TESTS
    TEST(ScreenshotPngTest, PreservesColorsAlphaRowOrderAndPaddingForAllSupportedFormats) {
        using namespace VSL_NAMESPACE::data_format;
        constexpr std::array<unsigned char, 16> rgba{255, 10, 20, 64, 30, 240, 40, 128,
                                                     50, 60, 230, 192, 70, 80, 90, 255};
        constexpr std::array<unsigned char, 16> bgra{20, 10, 255, 64, 40, 240, 30, 128,
                                                     230, 60, 50, 192, 90, 80, 70, 255};
        aho_test::TempDirectory directory;
        for (const auto format : {UNSIGNED_NORMALIZED8_RGBA, SRGB8_RGBA, UNSIGNED_NORMALIZED8_BGRA, SRGB8_BGRA}) {
            SCOPED_TRACE(static_cast<int>(format.type()));
            aho::ImagePixels input{2, 2, 12, format, std::vector<std::byte>(20, std::byte{0xEE})};
            const auto& bytes = format == UNSIGNED_NORMALIZED8_BGRA || format == SRGB8_BGRA ? bgra : rgba;
            for (unsigned y = 0; y < 2; ++y)
                for (unsigned x = 0; x < 8; ++x)
                    input.bytes[y * 12 + x] = std::byte{bytes[y * 8 + x]};
            const auto original = input.bytes;
            const auto path = directory.path / std::filesystem::path(u8"スクリーンショット.png");
            aho::x::save_screenshot(input, path);
            const auto actual = load_png(path);
            EXPECT_EQ(actual.width, 2);
            EXPECT_EQ(actual.height, 2);
            EXPECT_EQ(actual.bytes, (std::vector<unsigned char>(rgba.begin(), rgba.end())));
            EXPECT_EQ(input.bytes, original);
        }
    }

    TEST(ScreenshotPngTest, RejectsInvalidStorageDimensionsAndFormatsWithoutTouchingOutput) {
        aho_test::TempDirectory directory;
        const auto path = directory.path / "existing.png";
        {
            std::ofstream stream(path);
            stream << "keep this file";
        }
        const aho::ImagePixels valid{2, 2, 8, VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA,
                                     std::vector<std::byte>(16)};
        for (unsigned mode = 0; mode < 8; ++mode) {
            SCOPED_TRACE(mode);
            auto input = valid;
            switch (mode) {
                case 0:
                    input.width = 0;
                    break;
                case 1:
                    input.height = 0;
                    break;
                case 2:
                    input.width = UINT32_MAX;
                    break;
                case 3:
                    input.height = UINT32_MAX;
                    break;
                case 4:
                    input.row_stride = 7;
                    break;
                case 5:
                    input.row_stride = SIZE_MAX;
                    break;
                case 6:
                    input.bytes.resize(15);
                    break;
                case 7:
                    input.format = VSL_NAMESPACE::data_format::UNSIGNED_INT;
                    break;
            }
            EXPECT_THROW(aho::x::save_screenshot(input, path), std::invalid_argument);
        }
        std::ifstream stream(path);
        EXPECT_EQ((std::string{std::istreambuf_iterator<char>(stream), {}}), "keep this file");
    }

    TEST(ScreenshotPngTest, ReportsAnUnwritableOutputPath) {
        aho_test::TempDirectory directory;
        const aho::ImagePixels pixels{1, 1, 4, VSL_NAMESPACE::data_format::SRGB8_RGBA, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
        EXPECT_THROW(aho::x::save_screenshot(pixels, directory.path / "missing" / "image.png"), std::runtime_error);
        EXPECT_THROW(aho::x::save_screenshot(pixels, directory.path), std::runtime_error);
    }
#else
    class ScreenshotWindowIntegration : public aho_test::LiveGPUTest {
    protected:
        void require_readback();
        auto clear(float red, float green, float blue);
    };

    void ScreenshotWindowIntegration::require_readback() {
        start_engine();
        const auto window = engine->get_window();
        const auto device = engine->data_->logical_device.data_;
        const auto caps = device->parent_device->device.getSurfaceCapabilitiesKHR(window.data2->surface->data_->surface);
        if (!(caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc))
            GTEST_SKIP() << "Surface does not support TransferSrc";
    }
    auto ScreenshotWindowIntegration::clear(float red, float green, float blue) {
        return [=, this](auto& stream) {
            auto window = engine->get_window();
            auto begin = vsl::command::RenderPassBegin(window.data2->render_pass, window.data2->frame_buffer);
            begin.clear_values[0] = vsl::ClearColor{red, green, blue, 1};
            stream << begin;
        };
    }

    TEST_F(ScreenshotWindowIntegration, AddingReadbackUsageNotifiesWindowHooksOnceAndStillCaptures) {
        ASSERT_NO_FATAL_FAILURE(require_readback());
        if (IsSkipped())
            return;
        auto window = engine->get_window();
        ASSERT_FALSE(vsl::contain(window.data2->swapchain.images.front().get_usage(), vsl::ImageType::TRANSFER_SRC));
        const auto previous = window.data2->frame_buffer.data_;
        int calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++calls;
            EXPECT_NE(current->data2->frame_buffer.data_, previous);
            EXPECT_TRUE(vsl::contain(current->data2->swapchain.images.front().get_usage(), vsl::ImageType::TRANSFER_SRC));
        });
        aho::x::Screenshot screenshot(window);
        EXPECT_EQ(calls, 1);
        aho::x::Screenshot second(window);
        EXPECT_EQ(calls, 1);
        {
            aho::DrawPhase phase(*engine, clear(0, 1, 0));
            ASSERT_TRUE(screenshot.capture(phase));
        }
        const auto pixels = screenshot.read();
        EXPECT_EQ(pixels.width, window.frame_size().value.x.value);
        EXPECT_EQ(pixels.height, window.frame_size().value.y.value);
        EXPECT_EQ(pixels.bytes[1], std::byte{255});
        EXPECT_EQ(pixels.bytes[3], std::byte{255});
    }

    TEST_F(ScreenshotWindowIntegration, CapturesTheRequestedFrameAndSavesPngWithSharedBusyState) {
        ASSERT_NO_FATAL_FAILURE(require_readback());
        if (IsSkipped())
            return;
        auto window = engine->get_window();
        aho::x::Screenshot screenshot(window);
        auto alias = screenshot;
        EXPECT_FALSE(screenshot.ready());
        EXPECT_THROW(screenshot.read(0), std::logic_error);
        {
            aho::DrawPhase phase(*engine, clear(1, 0, 0));
            ASSERT_TRUE(screenshot.capture(phase));
            EXPECT_FALSE(alias.capture(phase));
            EXPECT_FALSE(screenshot.ready());
            EXPECT_THROW(screenshot.read(0), std::logic_error);
        }
        const auto raw = alias.read();
        EXPECT_TRUE(screenshot.ready());
        const auto extent = window.frame_size();
        EXPECT_EQ(raw.width, extent.value.x.value);
        EXPECT_EQ(raw.height, extent.value.y.value);
        aho_test::TempDirectory directory;
        const auto path = directory.path / "frame.png";
        screenshot.save(path, 0);
        const auto png = load_png(path);
        ASSERT_EQ(png.bytes.size(), std::size_t(raw.width) * raw.height * 4);
        for (std::size_t i = 0; i < png.bytes.size(); i += 4)
            ASSERT_EQ((std::array<unsigned char, 4>{png.bytes[i], png.bytes[i + 1], png.bytes[i + 2], png.bytes[i + 3]}),
                      (std::array<unsigned char, 4>{255, 0, 0, 255}));
    }

    TEST_F(ScreenshotWindowIntegration, PreservesCustomFinishAndExplicitSplitSubmission) {
        ASSERT_NO_FATAL_FAILURE(require_readback());
        if (IsSkipped())
            return;
        aho::x::Screenshot screenshot(engine->get_window());
        for (unsigned frame = 0; frame < 3; ++frame) {
            unsigned finishes = 0;
            {
                aho::DrawPhase phase(*engine, clear(0, 1, 0), [&](auto& stream) {
                    ++finishes;
                    stream << vsl::command::RenderPassEnd{};
                });
                ASSERT_TRUE(screenshot.capture(phase));
                EXPECT_EQ(phase.submit_commands(), vsl::ComputePhase::SubmitResult::SUCCESS);
                EXPECT_EQ(finishes, 1u);
                EXPECT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::SUCCESS);
                EXPECT_TRUE(phase.completion().wait());
                EXPECT_THROW(screenshot.capture(phase), std::logic_error);
            }
            EXPECT_EQ(finishes, 1u);
            const auto pixels = screenshot.read(0);
            EXPECT_EQ(pixels.bytes[1], std::byte{255});
            EXPECT_EQ(pixels.bytes[3], std::byte{255});
        }
    }

    TEST_F(ScreenshotWindowIntegration, ResizeKeepsReadbackEnabledAndUsesTheNewFramebufferSize) {
        ASSERT_NO_FATAL_FAILURE(require_readback());
        if (IsSkipped())
            return;
        auto window = engine->get_window();
        aho::x::Screenshot screenshot(window);
        for (const auto size : {48, 80}) {
            ASSERT_TRUE(window.resize(size, size / 2));
            EXPECT_TRUE(vsl::contain(window.data2->swapchain.images.front().get_usage(), vsl::ImageType::TRANSFER_SRC));
            {
                aho::DrawPhase phase(*engine, clear(0, 0, 1));
                ASSERT_TRUE(screenshot.capture(phase));
            }
            const auto pixels = screenshot.read();
            const auto actual = window.frame_size();
            EXPECT_EQ(pixels.width, actual.value.x.value);
            EXPECT_EQ(pixels.height, actual.value.y.value);
        }
    }

    TEST_F(ScreenshotWindowIntegration, CancelledAndThrowingFramesCanBeFollowedByAnotherCapture) {
        ASSERT_NO_FATAL_FAILURE(require_readback());
        if (IsSkipped())
            return;
        aho::x::Screenshot screenshot(engine->get_window());
        {
            aho::DrawPhase phase(*engine);
            ASSERT_TRUE(screenshot.capture(phase));
            phase.cancel();
        }
        EXPECT_FALSE(screenshot.ready());
        EXPECT_THROW(screenshot.read(0), std::logic_error);
        {
            aho::DrawPhase phase(*engine, nullptr, [](auto&) { throw std::runtime_error("finish failed"); });
            ASSERT_TRUE(screenshot.capture(phase));
            EXPECT_THROW(phase.submit(), std::runtime_error);
        }
        EXPECT_FALSE(screenshot.ready());
        EXPECT_THROW(screenshot.read(0), std::logic_error);
        {
            aho::DrawPhase phase(*engine, clear(0, 1, 0));
            ASSERT_TRUE(screenshot.capture(phase));
        }
        EXPECT_EQ(screenshot.read().bytes[1], std::byte{255});
    }

    TEST_F(ScreenshotWindowIntegration, RejectsCreationDuringDrawingAndCaptureForAnotherWindow) {
        ASSERT_NO_FATAL_FAILURE(require_readback());
        if (IsSkipped())
            return;
        auto window = engine->get_window();
        aho::Window other("Other screenshot window", 48, 32, *engine);
        aho::x::Screenshot screenshot(other);
        {
            aho::DrawPhase phase(*engine);
            EXPECT_THROW((aho::x::Screenshot(window)), std::logic_error);
            EXPECT_THROW(screenshot.capture(phase), std::invalid_argument);
        }
        other.close();
        EXPECT_THROW((aho::x::Screenshot(other)), std::invalid_argument);
    }
#endif
} // namespace
