#include "scene.hpp"
#include <AHO/engine/standard_engine.hpp>
#include <AHO/x/screenshot.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>

namespace {
    struct Options {
        size_t frames = 0;
        std::filesystem::path verify;
        bool help = false;
    };
    struct WindowGuard {
        aho::Window window;
        ~WindowGuard();
    };

    Options parse_options(int argc, char** argv);
    bool draw_frame(aho::StandardEngine& engine, typed_example::Scene& scene, float time, aho::x::Screenshot* capture = nullptr);
    aho::ImagePixels capture_frame(aho::StandardEngine& engine, typed_example::Scene& scene, aho::x::Screenshot& screenshot,
                                   const std::filesystem::path& path, float time);
    void check_colored_quads(const aho::ImagePixels& pixels);
    void verify(aho::StandardEngine& engine, aho::Window window, typed_example::Scene& scene,
                aho::x::Screenshot& screenshot, const std::filesystem::path& directory);
    int run(const Options& options);

    WindowGuard::~WindowGuard() {
        try {
            window.render_context().commands().wait_idle();
        } catch (...) {
        }
        window.close();
    }

    Options parse_options(int argc, char** argv) {
        Options result;
        for (int i = 1; i < argc; ++i) {
            const std::string_view argument(argv[i]);
            if (argument == "--help") {
                result.help = true;
            } else if (argument == "--verify" && i + 1 < argc) {
                result.verify = argv[++i];
            } else if (argument == "--frames" && i + 1 < argc) {
                const std::string_view value(argv[++i]);
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result.frames);
                if (error != std::errc{} || end != value.data() + value.size() || !result.frames)
                    throw std::invalid_argument("--frames requires a positive integer");
            } else {
                throw std::invalid_argument("Usage: aho-typed-example [--frames N | --verify DIRECTORY]");
            }
        }
        if (result.frames && !result.verify.empty())
            throw std::invalid_argument("Use either --frames or --verify");
        return result;
    }

    bool draw_frame(aho::StandardEngine& engine, typed_example::Scene& scene, float time, aho::x::Screenshot* capture) {
        aho::DrawPhase phase(engine);
        if (phase.state != vsl::ComputePhase::State::WAIT_SUBMIT)
            return false;
        scene.record(phase, time);
        if (capture && !capture->capture(phase))
            throw std::runtime_error("Screenshot request was not accepted");
        const auto result = phase.submit();
        if (result == vsl::ComputePhase::SubmitResult::WANT_RECREATE_SWAPCHAIN) {
            // このPhaseを終了し、次の反復でWindowからswapchainを再生成する。
            return false;
        }
        if (result != vsl::ComputePhase::SubmitResult::SUCCESS)
            throw std::runtime_error("Typed frame submission failed");
        return true;
    }

    aho::ImagePixels capture_frame(aho::StandardEngine& engine, typed_example::Scene& scene, aho::x::Screenshot& screenshot,
                                   const std::filesystem::path& path, float time) {
        for (int attempt = 0; attempt < 60; ++attempt) {
            if (!aho::Window::Update() || !engine.get_window())
                throw std::runtime_error("Verification window was closed");
            if (draw_frame(engine, scene, time, &screenshot)) {
                auto pixels = screenshot.read();
                check_colored_quads(pixels);
                screenshot.save(path);
                return pixels;
            }
            engine.get_window().recreate_swapchain();
        }
        throw std::runtime_error("Could not acquire a verification frame");
    }

    void check_colored_quads(const aho::ImagePixels& pixels) {
        std::array<size_t, 2> colored{};
        for (size_t y = 0; y < pixels.height; ++y)
            for (size_t x = 0; x < pixels.width; ++x) {
                const auto offset = y * pixels.row_stride + x * 4;
                const auto* rgb = pixels.bytes.data() + offset;
                if (std::to_integer<unsigned>(rgb[0]) > 48 || std::to_integer<unsigned>(rgb[1]) > 48 || std::to_integer<unsigned>(rgb[2]) > 48)
                    ++colored[x >= pixels.width / 2];
            }
        for (const auto count : colored)
            if (count < size_t(pixels.width) * pixels.height / 30)
                throw std::runtime_error("A Typed indexed draw did not produce its colored quad");
    }

    void verify(aho::StandardEngine& engine, aho::Window window, typed_example::Scene& scene,
                aho::x::Screenshot& screenshot, const std::filesystem::path& directory) {
        std::filesystem::create_directories(directory);
        // 読み戻しを待たずに送信し、すべてのcommand slotを繰り返し再利用する。
        const auto target = scene.frame_count() * 4;
        size_t submitted = 0;
        for (size_t attempt = 0; attempt < target + 60 && submitted < target; ++attempt) {
            if (!aho::Window::Update() || !window)
                throw std::runtime_error("Verification window was closed");
            if (draw_frame(engine, scene, float(submitted) * .1f))
                ++submitted;
            else
                window.recreate_swapchain();
        }
        if (submitted != target)
            throw std::runtime_error("Could not submit the frame resource reuse sequence");
        const auto first = capture_frame(engine, scene, screenshot, directory / "01-typed.png", 0);
        const auto rotated = capture_frame(engine, scene, screenshot, directory / "02-rotated.png", 1);
        if (first.width != rotated.width || first.height != rotated.height)
            throw std::runtime_error("Window dimensions changed during animation verification");
        size_t changed = 0;
        for (size_t i = 0; i < first.bytes.size(); i += 4)
            if (!std::equal(first.bytes.begin() + i, first.bytes.begin() + i + 4, rotated.bytes.begin() + i))
                ++changed;
        if (changed < size_t(first.width) * first.height / 100)
            throw std::runtime_error("Per-frame UBO updates did not change the rendered image");
        const auto builds = scene.pipeline_builds();
        const auto context = window.render_context();
        const auto format = context.format();
        window.resize(1000, 680);
        const auto resized = capture_frame(engine, scene, screenshot, directory / "03-resized.png", 1);
        if (first.width == resized.width && first.height == resized.height)
            throw std::runtime_error("Resize did not change the framebuffer dimensions");
        if (format == context.format() && scene.pipeline_builds() != builds)
            throw std::runtime_error("Compatible resize rebuilt the Typed pipeline");
        std::cout << "Verification passed: " << scene.frame_count() << " frame slots, " << submitted
                  << " submissions before readback, 2 colored draws, " << changed << " UBO-animated pixels, resize "
                  << first.width << 'x' << first.height << " -> " << resized.width << 'x' << resized.height
                  << ", pipeline builds=" << scene.pipeline_builds() << '\n';
    }

    int run(const Options& options) {
        aho::StandardEngine engine("Typed | frame UBO + camera + push + indexed draw + batch");
        auto window = engine.get_window();
        WindowGuard close{window};
        window.resize(860, 540);
        std::optional<aho::x::Screenshot> screenshot;
        if (!options.verify.empty())
            screenshot.emplace(window);
        typed_example::Scene scene(window, AHO_TYPED_SHADER_DIR);
        if (screenshot) {
            verify(engine, window, scene, *screenshot, options.verify);
            return 0;
        }
        const auto start = std::chrono::steady_clock::now();
        size_t rendered = 0;
        while (aho::Window::Update() && window && (!options.frames || rendered < options.frames)) {
            const auto extent = window.frame_size();
            if (extent.value.x.value <= 0 || extent.value.y.value <= 0)
                continue;
            const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
            if (draw_frame(engine, scene, time))
                ++rendered;
            else
                window.recreate_swapchain();
        }
        return 0;
    }
} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) {
            std::cout << "Usage: aho-typed-example [--frames N | --verify DIRECTORY]\n";
            return 0;
        }
        return run(options);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
