#include "world.hpp"
#include <AHO/engine/standard_engine.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <AHO/io/input_manager.hpp>
#include <AHO/io/key.hpp>
#include <AHO/io/mouse.hpp>
#include <AHO/x/screenshot.hpp>
#include <AHO/x/scene/camera.hpp>
#include <AHO/x/scene/gltf_renderer.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <atomic>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <thread>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    std::atomic_bool validationError = false;
    vsl::logger_t previousLogger = nullptr;
    void captureValidation(const char* text) {
        std::string_view message(text);
        if (message.find("VUID-") != message.npos || message.find("Validation Error") != message.npos)
            validationError = true;
        if (previousLogger)
            previousLogger(text);
    }
    struct ValidationLog {
        ValidationLog() {
            previousLogger = vsl::logger;
            vsl::logger = captureValidation;
        }
        ~ValidationLog() { vsl::logger = previousLogger; }
    };
    struct WindowCloseGuard {
        aho::Window window;
        ~WindowCloseGuard() {
            if (window)
                window.close();
        }
    };
    struct Options {
        bool selfTest = false;
        unsigned frames = 0;
        int view = 0;
        std::filesystem::path verify;
    };
    Options options(int argc, char** argv) {
        Options out;
        for (int i = 1; i < argc; ++i) {
            std::string_view flag(argv[i]);
            auto value = [&]() -> std::string_view {
                if (++i >= argc)
                    throw std::invalid_argument("Missing argument value");
                return argv[i];
            };
            auto number = [&]() {
                auto text = value();
                unsigned result = 0;
                auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
                if (error != std::errc{} || end != text.data() + text.size())
                    throw std::invalid_argument("Expected unsigned integer");
                return result;
            };
            if (flag == "--self-test")
                out.selfTest = true;
            else if (flag == "--verify")
                out.verify = value();
            else if (flag == "--frames")
                out.frames = number();
            else if (flag == "--view") {
                auto view = number();
                if (view < 1 || view > 3)
                    throw std::invalid_argument("View must be 1, 2 or 3");
                out.view = int(view - 1);
            } else if (flag == "--help") {
                std::cout << "aho-wander [--view 1|2|3] [--frames N] [--verify directory] [--self-test]\n"
                          << "WASD: walk | Shift: run | click: capture mouse | arrows: look\n"
                          << "1/2/3: lake / snow / aurora | T: day/night | H: HUD | F12: screenshot\n"
                          << "Esc: release mouse | Q: quit\n";
                std::exit(0);
            } else
                throw std::invalid_argument("Unknown option: " + std::string(flag));
        }
        return out;
    }
    struct Scene {
        aho::Mat4x4<float> viewProjection;
        glm::vec4 eyeTime, settings, forward, right;
    };
    static_assert(sizeof(Scene) == 128);
    // InputManager owns all sampled key/button transitions; no GLFW calls in the game.
    struct Controls {
        using Code = aho::input::KeyCode;
        aho::InputManager manager;
        std::unordered_map<Code, std::shared_ptr<aho::input::Key>> keys;
        std::shared_ptr<aho::input::Cursor> cursor;
        std::shared_ptr<aho::input::MouseButton> click;
        explicit Controls(aho::Window window) : manager(window) {
            for (auto code : {Code::W, Code::A, Code::S, Code::D, Code::LeftShift, Code::RightShift,
                              Code::Left, Code::Right, Code::Up, Code::Down, Code::Num1, Code::Num2, Code::Num3,
                              Code::T, Code::H, Code::F12, Code::Escape, Code::Q})
                keys.emplace(code, manager.get<aho::input::Key>(code));
            cursor = manager.get<aho::input::Cursor>();
            click = manager.get<aho::input::MouseButton>(aho::input::MouseCode::LeftClick);
        }
        bool held(Code code) const { return keys.at(code)->pressed(); }
        bool down(Code code) const { return keys.at(code)->down(); }
        void update(wander::Walker& walker, const wander::World& world, float seconds, bool& hud, float& targetNight) {
            manager.update();
            if (!cursor->focused()) {
                if (cursor->captured())
                    cursor->setCaptured(false);
                return;
            }
            if (down(Code::Escape))
                cursor->setCaptured(false);
            if (click->down())
                cursor->setCaptured(true);
            for (auto [code, index] : {std::pair{Code::Num1, 0}, {Code::Num2, 1}, {Code::Num3, 2}})
                if (down(code)) {
                    walker.view(index);
                    targetNight = walker.night;
                }
            if (down(Code::T))
                targetNight = targetNight < .5f ? 1.f : 0.f;
            if (down(Code::H))
                hud = !hud;
            if (cursor->captured()) {
                auto delta = cursor->delta();
                walker.yaw += float(delta.value.x.value) * .0022f;
                walker.pitch -= float(delta.value.y.value) * .0022f;
            }
            walker.yaw += float(held(Code::Right) - held(Code::Left)) * seconds * 1.4f;
            walker.pitch += float(held(Code::Up) - held(Code::Down)) * seconds * 1.1f;
            walker.pitch = std::clamp(walker.pitch, -1.3f, 1.3f);
            walker.walk(world, {float(held(Code::D) - held(Code::A)), float(held(Code::W) - held(Code::S))}, seconds, held(Code::LeftShift) || held(Code::RightShift));
        }
    };
    void checkImage(const aho::ImagePixels& pixels) {
        std::set<std::array<std::byte, 3>> colors;
        std::size_t varied = 0;
        for (unsigned y = 0; y < pixels.height; y += 3)
            for (unsigned x = 0; x < pixels.width; x += 3) {
                auto* p = pixels.bytes.data() + y * pixels.rowStride + x * 4;
                colors.insert({p[0], p[1], p[2]});
                if (p[0] != p[1] || p[1] != p[2])
                    ++varied;
            }
        std::cout << "GPU image: " << pixels.width << 'x' << pixels.height << ", " << colors.size() << " sampled colors\n";
        if (colors.size() < 150 || varied < std::size_t(pixels.width) * pixels.height / 50)
            throw std::runtime_error("GPU frame lacks expected terrain/sky color variation");
    }
    void checkChanged(const aho::ImagePixels& a, const aho::ImagePixels& b) {
        if (a.width != b.width || a.height != b.height)
            throw std::runtime_error("Cannot compare different frame dimensions");
        std::size_t changed = 0;
        for (unsigned y = 0; y < a.height; ++y)
            for (unsigned x = 0; x < a.width; ++x) {
                auto i = y * a.rowStride + x * 4, j = y * b.rowStride + x * 4;
                if (a.bytes[i] != b.bytes[j] || a.bytes[i + 1] != b.bytes[j + 1] || a.bytes[i + 2] != b.bytes[j + 2])
                    ++changed;
            }
        if (changed < std::size_t(a.width) * a.height / 100)
            throw std::runtime_error("Camera/day-night change did not affect the GPU image");
        std::cout << "GPU changed pixels: " << changed << '\n';
    }
    int run(const Options& opt) {
        wander::World world;
        wander::selfTest(world);
        if (opt.selfTest)
            return 0;
        auto fox = aho::x::resource::GltfModel::load(AHO_WANDER_MODEL);
        std::size_t foxClip = 0;
        for (std::size_t i = 0; i < fox.animations().size(); ++i)
            if (fox.animations()[i].name == "Walk")
                foxClip = i;
        aho::StandardEngine engine("WANDER | WASD walk - Shift run - Click look - 1/2/3 vistas - T night - Esc release - Q quit");
        auto window = engine.getWindow();
        WindowCloseGuard closeWindow{window};
        window.resize(1280, 800);
        window.show();
        window.focus();
        {
            auto shader = std::filesystem::path(AHO_WANDER_SHADER_DIR);
            aho::x::scene::RenderTarget target(engine);
            aho::x::scene::MeshRenderer renderer(engine, target, world.vertices, shader / "world.vert.spv", shader / "world.frag.spv");
            aho::x::scene::GltfRenderer foxRenderer(engine, target, fox, shader / "model.vert.spv", shader / "model.frag.spv");
            aho::x::Screenshot screenshot(window);
            Controls input(window);
            wander::Walker walker;
            walker.view(opt.view);
            bool hud = true;
            auto draw = [&](float time, bool capture) {
                auto extent = window.frame_size();
                if (extent.value.x.value <= 0 || extent.value.y.value <= 0)
                    return false;
                auto size = target.extent();
                auto direction = walker.direction();
                auto eye = walker.eye();
                aho::x::scene::Camera camera;
                camera.eye = {eye.x, eye.y, eye.z};
                camera.direction = {direction.x, direction.y, direction.z};
                camera.aspect = float(size[0]) / size[1];
                Scene scene{camera.viewProjection(),
                            {walker.eye(), time},
                            {walker.night, float(size[0]) / size[1], hud ? 1.f : 0.f, input.cursor->captured() ? 1.f : 0.f},
                            {direction, float(size[1])},
                            {glm::normalize(glm::cross(direction, glm::vec3(0, 1, 0))), float(size[0])}};
                aho::DrawPhase phase(engine, [](auto&) {});
                if (phase.state != vsl::ComputePhase::State::WaitSubmit)
                    return false;
                target.begin(phase);
                renderer.record(phase, std::as_bytes(std::span(&scene, 1)));
                glm::vec2 foxPosition(4 + std::sin(time * .3f) * 3, 81 + std::cos(time * .3f) * 1.5f);
                auto instance = glm::translate(glm::mat4(1), {foxPosition.x, wander::ground(foxPosition.x, foxPosition.y), foxPosition.y});
                instance = glm::rotate(instance, std::atan2(std::cos(time * .3f) * 3, -std::sin(time * .3f) * 1.5f), glm::vec3(0, 1, 0));
                instance = glm::scale(instance, glm::vec3(.014f));
                aho::x::resource::GltfMatrix foxWorld, viewProjection;
                std::copy_n(glm::value_ptr(instance), 16, foxWorld.begin());
                for (int c = 0; c < 4; ++c)
                    for (int r = 0; r < 4; ++r)
                        viewProjection[c * 4 + r] = scene.viewProjection[c][r];
                foxRenderer.record(phase, fox.sample(foxClip, time), foxWorld, viewProjection, {eye.x, eye.y, eye.z, walker.night});
                if (capture && !screenshot.capture(phase))
                    throw std::runtime_error("Screenshot already in flight");
                return true;
            };
            if (!opt.verify.empty()) {
                std::filesystem::create_directories(opt.verify);
                hud = false;
                auto capture = [&](const char* name, float time = 12.f) {
                    window.focus();
                    if (!aho::Window::Update())
                        throw std::runtime_error("Verification window was closed");
                    if (!draw(time, true))
                        throw std::runtime_error("Could not acquire verification frame");
                    auto pixels = screenshot.read();
                    screenshot.save(opt.verify / name);
                    checkImage(pixels);
                    return pixels;
                };
                walker.view(0);
                auto lake = capture("01-lake.png");
                auto start = walker.position;
                for (int i = 0; i < 90; ++i)
                    walker.walk(world, {1, 0}, 1.f / 60, false);
                if (glm::distance(start, walker.position) < 1)
                    throw std::runtime_error("Verification walk was obstructed");
                auto moved = capture("02-walk.png");
                checkChanged(lake, moved);
                walker.view(1);
                capture("03-snow.png");
                walker.view(2);
                auto aurora = capture("04-aurora.png");
                walker.night = 0;
                auto day = capture("05-day.png");
                checkChanged(aurora, day);
                walker.night = 1;
                auto animated = capture("06-aurora-later.png", 35.f);
                checkChanged(aurora, animated);
                for (unsigned i = 0; i < 16; ++i)
                    if (!draw(float(i), false))
                        throw std::runtime_error("Frame slot reuse failed");
                auto repeated = capture("07-aurora-repeat.png");
                if (repeated.bytes != aurora.bytes)
                    throw std::runtime_error("Repeated deterministic frame differs after slot reuse");
                window.resize(960, 600);
                capture("08-resized-first.png");
                auto resized = capture("08-resized.png");
                auto size = window.frame_size();
                if (resized.width != unsigned(size.value.x.value) || resized.height != unsigned(size.value.y.value))
                    throw std::runtime_error("Resized target dimensions are incorrect");
                window.resize(1100, 700);
                capture("08-enlarged.png");
                window.resize(960, 600);
                capture("08-shrunk-again.png");
                hud = true;
                capture("09-hud.png");
                std::cout << "GPU verified: walking, snow, day/night, animation, slot reuse and resize\n";
            } else {
                std::cout << "WASD walk | Shift run | Click look | arrows look | 1/2/3 vistas | T night | H HUD | F12 capture | Q quit\n";
                auto previous = std::chrono::steady_clock::now();
                const auto started = previous;
                float time = 12, targetNight = walker.night;
                unsigned rendered = 0, captures = 0;
                float titleElapsed = 0;
                unsigned titleFrames = 0;
                while (aho::Window::Update() && window) {
                    auto now = std::chrono::steady_clock::now();
                    float dt = std::min(std::chrono::duration<float>(now - previous).count(), .1f);
                    previous = now;
                    input.update(walker, world, dt, hud, targetNight);
                    if (input.cursor->focused() && input.down(Controls::Code::Q))
                        break;
                    walker.night = glm::mix(walker.night, targetNight, 1 - std::exp(-dt * 1.8f));
                    time += dt;
                    bool capture = input.cursor->focused() && input.down(Controls::Code::F12);
                    if (draw(time, capture)) {
                        ++rendered;
                        ++titleFrames;
                        if (capture) {
                            auto directory = std::filesystem::path("wander-captures");
                            std::filesystem::create_directories(directory);
                            auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
                            auto path = directory / ("wander-" + std::to_string(stamp) + "-" + std::to_string(++captures) + ".png");
                            screenshot.save(path);
                            std::cout << "Saved " << path << '\n';
                        }
                    } else
                        std::this_thread::sleep_for(std::chrono::milliseconds(16));
                    titleElapsed += dt;
                    if (titleElapsed > 1) {
                        window.set_title("WANDER | " + std::to_string(int(titleFrames / titleElapsed)) + " fps | WASD walk - Click look - Shift run - 1/2/3 vistas - T night - Q quit");
                        titleElapsed = 0;
                        titleFrames = 0;
                    }
                    if (opt.frames && rendered >= opt.frames)
                        break;
                }
                input.cursor->setCaptured(false);
                const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
                std::cout << "Rendered " << rendered << " frames in " << elapsed << " seconds (" << rendered / elapsed << " fps)\n";
            }
            engine._data->command_manager.waitIdle();
        }
        window.close();
        return 0;
    }
} // namespace
int main(int argc, char** argv) {
    try {
        const auto opt = options(argc, argv);
#ifdef AHO_WANDER_ICD_FILE
        const auto* useSystemDriver = std::getenv("AHO_WANDER_USE_SYSTEM_MOLTENVK");
        if (!opt.selfTest && (!useSystemDriver || std::string_view(useSystemDriver) != "1")) {
            if (!std::filesystem::exists(AHO_WANDER_ICD_FILE))
                throw std::runtime_error("Sample Vulkan driver is missing; run sample-game/run.sh to configure it");
            setenv("VK_ICD_FILENAMES", AHO_WANDER_ICD_FILE, 1);
            setenv("VK_DRIVER_FILES", AHO_WANDER_ICD_FILE, 1);
        }
#endif
        ValidationLog validation;
        int result = run(opt);
        if (validationError)
            throw std::runtime_error("Vulkan validation reported an error");
        return result;
    } catch (const std::exception& error) {
        std::cerr << "WANDER: " << error.what() << '\n';
        return 1;
    }
}
