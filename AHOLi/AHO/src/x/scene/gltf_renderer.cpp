#include <AHO/x/scene/gltf_renderer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <VSL/vsl.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace AHO_NAMESPACE::x::scene {
    namespace {
        namespace asset = AHO_NAMESPACE::x::resource;
        namespace pl = VSL_NAMESPACE::pipeline_layout;
        namespace cmd = VSL_NAMESPACE::command;
        namespace gfx = VSL_NAMESPACE::graphic_resource;
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
        using VertexBuffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::VertexBuffer, VSL_NAMESPACE::MemoryProperty::HostVisible | VSL_NAMESPACE::MemoryProperty::HostCoherent>;
        using IndexBuffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::IndexBuffer, VSL_NAMESPACE::MemoryProperty::HostVisible | VSL_NAMESPACE::MemoryProperty::HostCoherent>;
        using JointBuffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::StorageBuffer, VSL_NAMESPACE::MemoryProperty::HostVisible | VSL_NAMESPACE::MemoryProperty::HostCoherent>;
        using Texture = VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::TransferDst | VSL_NAMESPACE::ImageType::Sampled>;
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
        struct Mesh {
            int node;
            std::uint32_t count;
            VertexBuffer vertices;
            IndexBuffer indices;
            Texture image;
            VSL_NAMESPACE::Sampler sampler;
            bool skinned;
            std::vector<JointBuffer> joints;
            std::vector<gfx::Pool> pools;
            std::vector<gfx::Resource> resources;
            Mesh(AHO_NAMESPACE::engine::GraphicalEngine& engine, int node, const asset::GltfPrimitive& source, const asset::GltfModel& model, gfx::BindingLayout binding)
                : node(node), count(std::uint32_t(source.indices.size())), vertices(engine._data->logical_device, source.vertices.size() * sizeof(Vertex)),
                  indices(engine._data->logical_device, source.indices.size() * sizeof(std::uint32_t)),
                  image(makeTexture(engine, source, model)), sampler(engine._data->logical_device, VSL_NAMESPACE::SamplingMode::Linear, VSL_NAMESPACE::EdgeMode::Repeat),
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
                    joints.emplace_back(engine._data->logical_device, (jointCount + 1) * sizeof(Matrix));
                    resources[slot].update(image, sampler);
                    resources[slot].update(&joints.back(), 1, gfx::Type::StorageBuffer);
                }
            }
            void updateJoints(const asset::GltfModel& model, const asset::GltfPose& pose, std::size_t slot, std::array<float, 4> eyeNight) {
                // DrawPhase has waited for this command slot before its palette is overwritten.
                const Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                auto palette = skinned ? model.skinMatrices(pose, node) : std::vector{identity};
                Matrix metadata{};
                std::copy(eyeNight.begin(), eyeNight.end(), metadata.begin());
                palette.push_back(metadata);
                if (!joints.at(slot).copy(palette.data(), palette.size()))
                    throw std::runtime_error("Joint palette upload failed");
            }
            static Texture makeTexture(AHO_NAMESPACE::engine::GraphicalEngine& engine, const asset::GltfPrimitive& primitive, const asset::GltfModel& model) {
                asset::GltfImage white{"white", 1, 1, {255, 255, 255, 255}};
                const asset::GltfImage* source = &white;
                if (primitive.material >= 0) {
                    const auto& m = model.materials()[primitive.material];
                    if (m.baseColorTexture.texture >= 0)
                        source = &model.images()[model.textures()[m.baseColorTexture.texture].image];
                }
                Texture image(engine._data->logical_device, source->width, source->height, VSL_NAMESPACE::data_format::Srgb8RGBA);
                VSL_NAMESPACE::StagingBuffer<> staging(engine._data->logical_device, source->rgba.size());
                if (!staging.copy(source->rgba.data(), source->rgba.size()))
                    throw std::runtime_error("Texture staging failed");
                auto phase = engine._data->command_manager.startPhase<VSL_NAMESPACE::ComputePhase>();
                phase << cmd::ChangeImageBarrier(image, VSL_NAMESPACE::ImageLayout::TransferDstOptimal) << cmd::CopyBufferToImage(image, &staging, VSL_NAMESPACE::ImageLayout::TransferDstOptimal)
                      << cmd::ChangeImageBarrier(image, VSL_NAMESPACE::ImageLayout::TransferDstOptimal, VSL_NAMESPACE::ImageLayout::ShaderReadOnlyOptimal);
                if (phase.submit() != VSL_NAMESPACE::ComputePhase::SubmitResult::Success || !phase.completion().wait())
                    throw std::runtime_error("Texture transfer failed");
                return image;
            }
        };
    } // namespace
    struct GltfRenderer::Data {
        engine::GraphicalEngine& engine;
        RenderTarget& target;
        resource::GltfModel model;
        gfx::BindingLayout binding;
        std::vector<Mesh> meshes;
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Vertex> vertex;
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Fragment> fragment;
        std::optional<VSL_NAMESPACE::RenderPassAccessor> pass;
        std::optional<VSL_NAMESPACE::PipelineLayout> layout;
        std::optional<VSL_NAMESPACE::GraphicsPipeline> pipeline;
        VSL_NAMESPACE::Completion lastFrame;
        RenderTarget::Subscription subscription;
        Data(engine::GraphicalEngine& e, RenderTarget& t, resource::GltfModel m, const std::filesystem::path& vs, const std::filesystem::path& fs)
            : engine(e), target(t), model(std::move(m)),
              binding(e._data->logical_device, {{0, gfx::Type::CombinedImageSampler, VSL_NAMESPACE::ShaderFlag::Fragment}, {1, gfx::Type::StorageBuffer, VSL_NAMESPACE::ShaderFlag::Vertex}}),
              vertex(e._data->logical_device, vs), fragment(e._data->logical_device, fs) {
            if (target.window().engine._data != e._data)
                throw std::invalid_argument("glTF renderer and target use different engines");
            for (auto node : visibleNodes(model))
                if (model.nodes()[node].mesh >= 0)
                    for (const auto& primitive : model.meshes()[model.nodes()[node].mesh].primitives)
                        meshes.emplace_back(e, node, primitive, model, binding);
            if (meshes.empty())
                throw std::invalid_argument("glTF scene contains no drawable meshes");
            updatePipeline();
            subscription = target.onRecreated([this] { updatePipeline(); });
        }
        void updatePipeline() {
            auto next = target.renderPass();
            if (pass && pass->_data == next._data)
                return;
            pl::VertexInput input;
            input.add(pl::VertexInputShapeDefinition{
                .layouts = {{VSL_NAMESPACE::data_format::FloatRGB, 0, offsetof(Vertex, position)},
                            {VSL_NAMESPACE::data_format::FloatRGB, 1, offsetof(Vertex, normal)},
                            {VSL_NAMESPACE::data_format::FloatRG, 2, offsetof(Vertex, uv)},
                            {VSL_NAMESPACE::data_format::FloatRGBA, 3, offsetof(Vertex, color)},
                            {VSL_NAMESPACE::data_format::FloatRGBA, 4, offsetof(Vertex, joints)},
                            {VSL_NAMESPACE::data_format::FloatRGBA, 5, offsetof(Vertex, weights)}},
                .binding = 0,
                .size = sizeof(Vertex)});
            pl::PushConstants constants;
            constants.definitions = {{VSL_NAMESPACE::ShaderFlag::Vertex, 0, sizeof(Transform)}};
            auto swapchain = target.swapchain();
            VSL_NAMESPACE::PipelineLayout nextLayout(engine._data->logical_device, pl::ShaderGroup("AHO experimental glTF", {vertex, fragment}), input,
                           pl::ResourceBinding({binding}), constants, pl::ColorBlend{}, pl::InputAssembly{},
                           pl::Rasterization{pl::CullingMode::None}, pl::DepthStencil{}, pl::Multisample{}, pl::DynamicState{},
                           VSL_NAMESPACE::Viewport(swapchain), VSL_NAMESPACE::Scissor(swapchain));
            VSL_NAMESPACE::GraphicsPipeline nextPipeline(nextLayout, next);
            pipeline = std::move(nextPipeline);
            layout = std::move(nextLayout);
            pass = next;
        }
    };
    GltfRenderer::GltfRenderer(engine::GraphicalEngine& engine, RenderTarget& target, resource::GltfModel model,
                               const std::filesystem::path& vertex, const std::filesystem::path& fragment)
        : data(std::make_unique<Data>(engine, target, std::move(model), vertex, fragment)) {}
    GltfRenderer::~GltfRenderer() {
        if (data) {
            data->subscription = {};
            data->engine._data->command_manager.waitIdle();
        }
    }
    void GltfRenderer::record(DrawPhase& phase, const resource::GltfPose& pose, const resource::GltfMatrix& instance,
                              const resource::GltfMatrix& viewProjection, std::array<float, 4> eyeNight) {
        if (phase.engine->_data != data->engine._data || phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit)
            throw std::invalid_argument("glTF renderer requires a recording DrawPhase on the same engine");
        if (data->pass->_data != data->target.renderPass()._data || phase.swapchain._data != data->target.swapchain()._data)
            throw std::logic_error("glTF renderer has not received its target update");
        if (data->lastFrame._data == phase.completion()._data)
            throw std::logic_error("Use a separate GltfRenderer for each animated instance in a frame");
        data->lastFrame = phase.completion();
        auto swapchain = data->target.swapchain();
        auto stream = phase << *data->pipeline << VSL_NAMESPACE::Viewport(swapchain) << VSL_NAMESPACE::Scissor(swapchain);
        for (auto& mesh : data->meshes) {
            mesh.updateJoints(data->model, pose, phase.frameSlot, eyeNight);
            Transform transform{multiply(instance, pose.worldMatrices.at(mesh.node)), viewProjection};
            stream << cmd::PushConstant(*data->layout, VSL_NAMESPACE::ShaderFlag::Vertex, transform)
                   << cmd::BindGraphicResource(mesh.resources.at(phase.frameSlot), gfx::BindingDestination::Graphics, *data->pipeline)
                   << cmd::BindVertexBuffer(mesh.vertices) << cmd::BindIndexBuffer(mesh.indices) << cmd::DrawIndexed(mesh.count);
        }
    }
} // namespace AHO_NAMESPACE::x::scene
