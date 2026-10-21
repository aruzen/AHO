#include <AHO/drawer/draw_phase.hpp>
#include <AHO/engine/standard_engine.hpp>
#include <AHO/x/resource/gltf.hpp>
#include <AHO/x/screenshot.hpp>
#include <VSL/vsl.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>
#include <tuple>

namespace {
    std::atomic_bool validationError = false;
    vsl::logger_t previousLogger = nullptr;
    void captureValidation(const char* text) {
        const std::string_view message(text);
        if (message.find("VUID-") != std::string_view::npos || message.find("Validation Error") != std::string_view::npos)
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
    namespace asset = aho::x::resource;
    namespace pl = vsl::pipeline_layout;
    namespace cmd = vsl::command;
    namespace gfx = vsl::graphic_resource;
    using Matrix = asset::GltfMatrix;
    using Vec3 = std::array<float, 3>;
    struct Vertex {
        Vec3 position, normal;
        std::array<float, 2> uv;
        std::array<float, 4> color;
        std::array<float, 4> joints{}, weights{};
    };
    struct Transform {
        Matrix world, viewProjection;
    };
    static_assert(sizeof(Transform) == 128);
    using VertexBuffer = vsl::Buffer<vsl::MemoryType::VertexBuffer, vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>;
    using IndexBuffer = vsl::Buffer<vsl::MemoryType::IndexBuffer, vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>;
    using JointBuffer = vsl::Buffer<vsl::MemoryType::StorageBuffer, vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>;
    using Texture = vsl::Image<vsl::ImageType::TransferDst | vsl::ImageType::Sampled>;
    using Depth = vsl::Image<vsl::ImageType::DepthStencilAttachment>;

    Matrix multiply(const Matrix& a, const Matrix& b) {
        Matrix out{};
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                for (int k = 0; k < 4; ++k)
                    out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
        return out;
    }
    Vec3 point(const Matrix& m, const Vec3& v) {
        Vec3 out{};
        for (int r = 0; r < 3; ++r)
            out[r] = m[12 + r] + m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2];
        return out;
    }
    float dot(Vec3 a, Vec3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
    Vec3 normalized(Vec3 v) {
        const auto d = std::sqrt(dot(v, v));
        if (d <= 0)
            throw std::runtime_error("Invalid camera direction");
        for (auto& x : v)
            x /= d;
        return v;
    }
    Vec3 cross(Vec3 a, Vec3 b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
    struct Camera {
        Vec3 center{};
        float radius = 1;
        Matrix projection(unsigned width, unsigned height) const {
            const float scale = 1 / std::tan(45.f * 3.14159265f / 360), near = radius * 0.01f, far = radius * 20;
            Matrix p{};
            p[0] = scale * height / width;
            p[5] = -scale;
            p[10] = far / (near - far);
            p[11] = -1;
            p[14] = near * far / (near - far);
            Vec3 direction = normalized({3, 1, 1.5f});
            Vec3 eye{}, forward{};
            for (int i = 0; i < 3; ++i) {
                eye[i] = center[i] + direction[i] * radius * 3.4f;
                forward[i] = -direction[i];
            }
            auto side = normalized(cross(forward, {0, 1, 0})), up = cross(side, forward);
            Matrix view{side[0], up[0], -forward[0], 0, side[1], up[1], -forward[1], 0, side[2], up[2], -forward[2], 0, -dot(side, eye), -dot(up, eye), dot(forward, eye), 1};
            return multiply(p, view);
        }
    };
    std::vector<int> visibleNodes(const asset::GltfModel& model) {
        std::vector<int> nodes;
        if (model.defaultScene() >= 0)
            nodes = model.scenes()[std::size_t(model.defaultScene())].roots;
        else
            for (std::size_t i = 0; i < model.nodes().size(); ++i)
                if (model.nodes()[i].parent < 0)
                    nodes.push_back(int(i));
        for (std::size_t i = 0; i < nodes.size(); ++i)
            for (auto child : model.nodes()[std::size_t(nodes[i])].children)
                nodes.push_back(child);
        return nodes;
    }
    Camera frameModel(const asset::GltfModel& model, const std::vector<int>& nodes) {
        auto pose = model.restPose();
        Vec3 min, max;
        min.fill(std::numeric_limits<float>::max());
        max.fill(std::numeric_limits<float>::lowest());
        bool found = false;
        for (auto index : nodes) {
            const auto& n = model.nodes()[index];
            if (n.mesh < 0)
                continue;
            auto palette = n.skin >= 0 ? model.skinMatrices(pose, index) : std::vector<Matrix>{};
            for (const auto& primitive : model.meshes()[n.mesh].primitives)
                for (const auto& v : primitive.vertices) {
                    auto position = v.position;
                    if (n.skin >= 0 && primitive.hasSkin) {
                        position = {};
                        for (int j = 0; j < 4; ++j) {
                            auto jointPosition = point(palette.at(v.joints[j]), v.position);
                            for (int c = 0; c < 3; ++c)
                                position[c] += v.weights[j] * jointPosition[c];
                        }
                    }
                    auto p = point(pose.worldMatrices[index], position);
                    for (int c = 0; c < 3; ++c) {
                        min[c] = std::min(min[c], p[c]);
                        max[c] = std::max(max[c], p[c]);
                    }
                    found = true;
                }
        }
        if (!found)
            throw std::runtime_error("Selected scene has no mesh");
        Camera camera;
        Vec3 diagonal;
        for (int c = 0; c < 3; ++c) {
            camera.center[c] = (min[c] + max[c]) * 0.5f;
            diagonal[c] = max[c] - min[c];
        }
        camera.radius = std::max(std::sqrt(dot(diagonal, diagonal)) * 0.5f, 0.001f);
        return camera;
    }
    struct Mesh {
        int node;
        std::uint32_t count;
        VertexBuffer vertices;
        IndexBuffer indices;
        Texture image;
        vsl::Sampler sampler;
        bool skinned;
        std::vector<JointBuffer> joints;
        std::vector<gfx::Pool> pools;
        std::vector<gfx::Resource> resources;
        Mesh(aho::StandardEngine& engine, int node, const asset::GltfPrimitive& source, const asset::GltfModel& model, gfx::BindingLayout binding)
            : node(node), count(std::uint32_t(source.indices.size())), vertices(engine._data->logical_device, source.vertices.size() * sizeof(Vertex)),
              indices(engine._data->logical_device, source.indices.size() * sizeof(std::uint32_t)),
              image(makeTexture(engine, source, model)), sampler(engine._data->logical_device, vsl::SamplingMode::Linear, vsl::EdgeMode::Repeat),
              skinned(model.nodes()[node].skin >= 0 && source.hasSkin) {
            const asset::GltfMaterial material = source.material < 0 ? asset::GltfMaterial{} : model.materials()[source.material];
            if (source.topology != asset::GltfTopology::Triangles || material.alphaMode != asset::GltfAlphaMode::Opaque)
                throw std::runtime_error("Example requires opaque triangle primitives");
            std::vector<Vertex> data;
            data.reserve(source.vertices.size());
            for (const auto& v : source.vertices) {
                Vertex out{v.position, v.normal, {}, v.color};
                if (skinned) {
                    for (int j = 0; j < 4; ++j)
                        out.joints[j] = float(v.joints[j]);
                    out.weights = v.weights;
                }
                const auto& info = material.baseColorTexture;
                const auto& uv = v.texCoords[info.texture < 0 ? 0 : std::size_t(info.texCoord)];
                const float x = uv[0] * info.scale[0], y = uv[1] * info.scale[1];
                out.uv = {info.offset[0] + std::cos(info.rotation) * x - std::sin(info.rotation) * y, info.offset[1] + std::sin(info.rotation) * x + std::cos(info.rotation) * y};
                for (int c = 0; c < 4; ++c)
                    out.color[c] *= material.baseColor[c];
                data.push_back(out);
            }
            // glTF permits omitted normals. Accumulate area-weighted triangle normals.
            if (!source.hasNormals) {
                for (auto& v : data)
                    v.normal = {};
                for (std::size_t i = 0; i + 2 < source.indices.size(); i += 3) {
                    auto& a = data.at(source.indices[i]);
                    auto& b = data.at(source.indices[i + 1]);
                    auto& c = data.at(source.indices[i + 2]);
                    Vec3 ab{}, ac{};
                    for (int k = 0; k < 3; ++k) {
                        ab[k] = b.position[k] - a.position[k];
                        ac[k] = c.position[k] - a.position[k];
                    }
                    auto normal = cross(ab, ac);
                    for (auto* v : {&a, &b, &c})
                        for (int k = 0; k < 3; ++k)
                            v->normal[k] += normal[k];
                }
                for (auto& v : data)
                    v.normal = dot(v.normal, v.normal) > 0 ? normalized(v.normal) : Vec3{0, 1, 0};
            }
            if (!vertices.copy(data.data(), data.size()) || !indices.copy(source.indices.data(), source.indices.size()))
                throw std::runtime_error("Model buffer upload failed");
            const auto slots = engine._data->command_manager.getBuffer().getSize();
            std::tie(pools, resources) = engine._data->graphic_resource_manager->allocate(std::vector(slots, binding));
            const auto jointCount = skinned ? model.skins()[model.nodes()[node].skin].joints.size() : 1;
            joints.reserve(slots);
            for (std::size_t slot = 0; slot < slots; ++slot) {
                joints.emplace_back(engine._data->logical_device, jointCount * sizeof(Matrix));
                resources[slot].update(image, sampler);
                resources[slot].update(&joints.back(), 1, gfx::Type::StorageBuffer);
            }
        }
        void updateJoints(const asset::GltfModel& model, const asset::GltfPose& pose, std::size_t slot) {
            // DrawPhase has waited for this command slot before its palette is overwritten.
            const Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            auto palette = skinned ? model.skinMatrices(pose, node) : std::vector{identity};
            if (!joints.at(slot).copy(palette.data(), palette.size()))
                throw std::runtime_error("Joint palette upload failed");
        }
        static Texture makeTexture(aho::StandardEngine& engine, const asset::GltfPrimitive& primitive, const asset::GltfModel& model) {
            asset::GltfImage white{"white", 1, 1, {255, 255, 255, 255}};
            const asset::GltfImage* source = &white;
            if (primitive.material >= 0) {
                const auto& m = model.materials()[primitive.material];
                if (m.baseColorTexture.texture >= 0)
                    source = &model.images()[model.textures()[m.baseColorTexture.texture].image];
            }
            Texture image(engine._data->logical_device, source->width, source->height, vsl::data_format::Srgb8RGBA);
            vsl::StagingBuffer<> staging(engine._data->logical_device, source->rgba.size());
            if (!staging.copy(source->rgba.data(), source->rgba.size()))
                throw std::runtime_error("Texture staging failed");
            auto phase = engine._data->command_manager.startPhase<vsl::ComputePhase>();
            phase << cmd::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal) << cmd::CopyBufferToImage(image, &staging, vsl::ImageLayout::TransferDstOptimal)
                  << cmd::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal, vsl::ImageLayout::ShaderReadOnlyOptimal);
            if (phase.submit() != vsl::ComputePhase::SubmitResult::Success || !phase.completion().wait())
                throw std::runtime_error("Texture transfer failed");
            return image;
        }
    };
    struct Targets {
        vsl::SwapchainAccessor swapchain;
        vsl::RenderPass pass;
        std::vector<Depth> depths;
        std::vector<vsl::FrameBuffer> frames;
        unsigned width, height;
        static vsl::RenderPass makePass(vsl::LogicalDeviceAccessor device, vsl::SwapchainAccessor swapchain) {
            vsl::RenderPassDescription description;
            description.addColor({.format = swapchain.getFormat(), .finalLayout = vsl::ImageLayout::Present});
            description.setDepthStencil({.format = vsl::data_format::Depth32Float, .store = vsl::StoreOp::DontCare, .finalLayout = vsl::ImageLayout::DepthStencilAttachmentOptimal});
            return {device, std::move(description)};
        }
        Targets(aho::StandardEngine& engine, aho::Window window) : swapchain(window._data2->swapchain), pass(makePass(engine._data->logical_device, swapchain)) {
            auto size = window.frame_size();
            width = size.value.x.value;
            height = size.value.y.value;
            for (auto image : swapchain.images) {
                depths.emplace_back(engine._data->logical_device, width, height, vsl::data_format::Depth32Float);
                frames.emplace_back(pass, width, height, std::vector<vsl::ImageAccessor>{image, depths.back()});
            }
        }
    };
    struct Options {
        std::filesystem::path model = std::filesystem::path(AHO_GLTF_VIEWER_ASSET_DIR) / "Fox.glb";
        std::optional<std::string> animation;
        std::filesystem::path verify;
        std::optional<double> time;
        unsigned frames = 0;
    };
    Options options(int argc, char** argv) {
        Options out;
        for (int i = 1; i < argc; ++i) {
            std::string flag = argv[i];
            auto value = [&]() {
                if (i + 1 >= argc)
                    throw std::invalid_argument("Missing value for " + flag);
                return std::string(argv[++i]);
            };
            if (flag == "--model")
                out.model = value();
            else if (flag == "--animation")
                out.animation = value();
            else if (flag == "--verify")
                out.verify = value();
            else if (flag == "--time")
                out.time = std::stod(value());
            else if (flag == "--frames") {
                const auto input = value();
                std::size_t consumed = 0;
                auto count = std::stoul(input, &consumed);
                if (input.empty() || input.front() == '-' || consumed != input.size() || count > std::numeric_limits<unsigned>::max())
                    throw std::invalid_argument("Invalid frame count");
                out.frames = static_cast<unsigned>(count);
            } else
                throw std::invalid_argument("Usage: aho-gltf-viewer [--model path] [--animation name] [--time seconds] [--frames count] [--verify output-directory]");
        }
        if (out.time && !std::isfinite(*out.time))
            throw std::invalid_argument("Time must be finite");
        return out;
    }
    void verifyFrames(const aho::ImagePixels& first, const aho::ImagePixels& second) {
        if (first.width != second.width || first.height != second.height)
            throw std::runtime_error("Verification capture dimensions changed unexpectedly");
        std::size_t foreground = 0, changed = 0;
        std::set<std::array<std::byte, 3>> colors;
        for (std::size_t y = 0; y < first.height; ++y)
            for (std::size_t x = 0; x < first.width; ++x) {
                auto a = first.bytes.data() + y * first.rowStride + x * 4;
                auto b = second.bytes.data() + y * second.rowStride + x * 4;
                if (std::memcmp(a, first.bytes.data(), 3) != 0) {
                    ++foreground;
                    colors.insert({a[0], a[1], a[2]});
                }
                if (std::memcmp(a, b, 3) != 0)
                    ++changed;
            }
        const auto total = std::size_t(first.width) * first.height;
        if (foreground < total / 50 || changed < total / 200 || colors.size() < 32)
            throw std::runtime_error("GPU verification failed: model, texture or animation is missing");
        std::cout << "GPU verified: foreground=" << foreground << " changed=" << changed << " colors=" << colors.size() << '\n';
    }
    int run(const Options& opt) {
        auto model = asset::GltfModel::load(opt.model);
        auto nodes = visibleNodes(model);
        auto camera = frameModel(model, nodes);
        if (model.animations().empty())
            throw std::runtime_error("Choose a model containing an animation");
        std::size_t animation = 0;
        for (std::size_t i = 0; i < model.animations().size(); ++i)
            if (model.animations()[i].name == opt.animation.value_or("Run"))
                animation = i;
        if (opt.animation && model.animations()[animation].name != *opt.animation)
            throw std::invalid_argument("Animation name not found: " + *opt.animation);
        aho::StandardEngine engine("AHO glTF - " + model.animations()[animation].name + " - Space: pause / Esc: close");
        auto window = engine.getWindow();
        window.resize(800, 640);
        std::cout << "Model: " << opt.model << " | animation: " << model.animations()[animation].name << std::endl;
        {
            aho::x::Screenshot screenshot(window);
            gfx::BindingLayout textureBinding(engine._data->logical_device, {{0, gfx::Type::CombinedImageSampler, vsl::ShaderFlag::Fragment}, {1, gfx::Type::StorageBuffer, vsl::ShaderFlag::Vertex}});
            std::vector<Mesh> meshes;
            for (auto node : nodes)
                if (model.nodes()[node].mesh >= 0)
                    for (const auto& p : model.meshes()[model.nodes()[node].mesh].primitives)
                        meshes.emplace_back(engine, node, p, model, textureBinding);
            vsl::Shader<vsl::ShaderType::Vertex> vertex(engine._data->logical_device, std::filesystem::path(AHO_GLTF_VIEWER_SHADER_DIR) / "gltf_viewer.vert.spv");
            vsl::Shader<vsl::ShaderType::Fragment> fragment(engine._data->logical_device, std::filesystem::path(AHO_GLTF_VIEWER_SHADER_DIR) / "gltf_viewer.frag.spv");
            std::optional<Targets> targets;
            std::optional<vsl::PipelineLayout> layout;
            std::optional<vsl::GraphicsPipeline> pipeline;
            auto rebuild = [&]() {
                engine._data->command_manager.waitIdle();
                pipeline.reset();
                layout.reset();
                targets.reset();
                targets.emplace(engine, window);
                pl::VertexInput input;
                input.add(pl::VertexInputShapeDefinition{
                    .layouts = {
                        {vsl::data_format::FloatRGB, 0, offsetof(Vertex, position)},
                        {vsl::data_format::FloatRGB, 1, offsetof(Vertex, normal)},
                        {vsl::data_format::FloatRG, 2, offsetof(Vertex, uv)},
                        {vsl::data_format::FloatRGBA, 3, offsetof(Vertex, color)},
                        {vsl::data_format::FloatRGBA, 4, offsetof(Vertex, joints)},
                        {vsl::data_format::FloatRGBA, 5, offsetof(Vertex, weights)}},
                    .binding = 0,
                    .size = sizeof(Vertex)});
                pl::PushConstants constants;
                constants.definitions = {{vsl::ShaderFlag::Vertex, 0, sizeof(Transform)}};
                layout.emplace(engine._data->logical_device, pl::ShaderGroup("gltf-viewer", {vertex, fragment}), input,
                               pl::ResourceBinding({textureBinding}), constants, pl::ColorBlend{}, pl::InputAssembly{},
                               pl::Rasterization{pl::CullingMode::None}, pl::DepthStencil{}, pl::Multisample{}, pl::DynamicState{},
                               vsl::Viewport(targets->swapchain), vsl::Scissor(targets->swapchain));
                pipeline.emplace(*layout, targets->pass);
            };
            rebuild();
            auto draw = [&](double time, bool capture) {
                if (targets->swapchain._data != window._data2->swapchain._data)
                    rebuild();
                auto pose = model.sample(animation, time);
                aho::DrawPhase phase(engine, [](auto&) {});
                if (phase.state != vsl::ComputePhase::State::WaitSubmit)
                    return false;
                phase << cmd::RenderPassBegin(targets->frames.at(phase.getImageIndex()), {vsl::ClearColor{0.025f, 0.035f, 0.06f, 1}, vsl::ClearDepthStencil{1, 0}});
                auto stream = phase << *pipeline << vsl::Viewport(targets->swapchain) << vsl::Scissor(targets->swapchain);
                for (auto& mesh : meshes) {
                    mesh.updateJoints(model, pose, phase.frameSlot);
                    Transform transform{pose.worldMatrices[mesh.node], camera.projection(targets->width, targets->height)};
                    stream << cmd::PushConstant(*layout, vsl::ShaderFlag::Vertex, transform) << cmd::BindGraphicResource(mesh.resources.at(phase.frameSlot), gfx::BindingDestination::Graphics, *pipeline) << cmd::BindVertexBuffer(mesh.vertices) << cmd::BindIndexBuffer(mesh.indices) << cmd::DrawIndexed(mesh.count);
                }
                if (capture && !screenshot.capture(phase))
                    throw std::runtime_error("Screenshot is busy");
                return true;
            };
            if (!opt.verify.empty()) {
                std::filesystem::create_directories(opt.verify);
                const auto& clip = model.animations()[animation];
                double t0 = clip.start, t1 = clip.start + (clip.end - clip.start) * 0.23;
                if (!draw(t0, true))
                    throw std::runtime_error("First verification frame could not be acquired");
                auto first = screenshot.read();
                screenshot.save(opt.verify / "frame-0.png");
                if (!draw(t1, true))
                    throw std::runtime_error("Second verification frame could not be acquired");
                auto second = screenshot.read();
                screenshot.save(opt.verify / "frame-1.png");
                verifyFrames(first, second);
                // Reuse every command slot without a readback wait between submissions.
                const auto slots = engine._data->command_manager.getBuffer().getSize();
                for (std::size_t frame = 0; frame < slots * 4; ++frame)
                    if (!draw(t0 + (clip.end - clip.start) * double(frame) / double(slots * 4), false))
                        throw std::runtime_error("Palette reuse frame could not be acquired");
                if (!draw(t0, true))
                    throw std::runtime_error("Repeated verification frame could not be acquired");
                auto repeated = screenshot.read();
                screenshot.save(opt.verify / "frame-repeat.png");
                if (first.width != repeated.width || first.height != repeated.height)
                    throw std::runtime_error("Repeated capture dimensions changed unexpectedly");
                for (std::size_t y = 0; y < first.height; ++y)
                    if (std::memcmp(first.bytes.data() + y * first.rowStride, repeated.bytes.data() + y * repeated.rowStride, first.width * 4) != 0)
                        throw std::runtime_error("Repeated pose differs after joint palette slot reuse");
                std::cout << "GPU verified: repeated pose identical after " << slots * 4 << " frames\n";
                window.resize(640, 480);
                if (!draw(t1, true))
                    throw std::runtime_error("Resized verification frame could not be acquired");
                auto resized = screenshot.read();
                screenshot.save(opt.verify / "resized.png");
                auto extent = window.frame_size();
                if (resized.width != unsigned(extent.value.x.value) || resized.height != unsigned(extent.value.y.value))
                    throw std::runtime_error("Resize verification failed");
            } else {
                auto previous = std::chrono::steady_clock::now();
                double time = opt.time.value_or(model.animations()[animation].start);
                bool paused = opt.time.has_value(), wasSpace = false;
                unsigned rendered = 0;
                while (aho::Window::Update() && window) {
                    auto* handle = static_cast<GLFWwindow*>(window._data->window_handle);
                    if (glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                        break;
                    bool space = glfwGetKey(handle, GLFW_KEY_SPACE) == GLFW_PRESS;
                    if (space && !wasSpace)
                        paused = !paused;
                    wasSpace = space;
                    auto now = std::chrono::steady_clock::now();
                    if (!paused)
                        time += std::chrono::duration<double>(now - previous).count();
                    previous = now;
                    auto extent = window.frame_size();
                    if (extent.value.x.value <= 0 || extent.value.y.value <= 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(16));
                        continue;
                    }
                    if (draw(time, false))
                        ++rendered;
                    if (opt.frames && rendered >= opt.frames)
                        break;
                }
            }
            engine._data->command_manager.waitIdle();
        }
        window.close();
        return 0;
    }
} // namespace
int main(int argc, char** argv) {
    try {
        ValidationLog validation;
        const auto result = run(options(argc, argv));
        if (validationError)
            throw std::runtime_error("Vulkan validation reported an error");
        return result;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
