#include <AHO/x/scene/mesh_renderer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <VSL/vsl.hpp>
#include <optional>
#include <limits>
#include <stdexcept>

namespace AHO_NAMESPACE::x::scene {
    namespace pl = VSL_NAMESPACE::pipeline_layout;
    namespace cmd = VSL_NAMESPACE::command;
    using Buffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::VertexBuffer, VSL_NAMESPACE::MemoryProperty::HostVisible | VSL_NAMESPACE::MemoryProperty::HostCoherent>;
    struct MeshRenderer::Data {
        engine::GraphicalEngine& engine;
        RenderTarget& target;
        Buffer vertices;
        unsigned count, pushBytes;
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Vertex> vertex;
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Fragment> fragment;
        std::optional<VSL_NAMESPACE::RenderPassAccessor> pass;
        std::optional<VSL_NAMESPACE::PipelineLayout> layout;
        std::optional<VSL_NAMESPACE::GraphicsPipeline> pipeline;
        RenderTarget::Subscription subscription;
        Data(engine::GraphicalEngine& e, RenderTarget& t, std::span<const MeshVertex> source, const std::filesystem::path& vs, const std::filesystem::path& fs, unsigned bytes)
            : engine(e), target(t), vertices(e._data->logical_device, source.size_bytes()), count(unsigned(source.size())), pushBytes(bytes),
              vertex(e._data->logical_device, vs), fragment(e._data->logical_device, fs) {
            if (target.window().engine._data != e._data)
                throw std::invalid_argument("Mesh renderer and target use different engines");
            if (!vertices.copy(source.data(), source.size()))
                throw std::runtime_error("Mesh vertex upload failed");
            updatePipeline();
            subscription = target.onRecreated([this] { updatePipeline(); });
        }
        void updatePipeline() {
            auto next = target.renderPass();
            if (pass && pass->_data == next._data)
                return;
            pl::VertexInput input;
            input.add(pl::VertexInputShapeDefinition{
                .layouts = {{VSL_NAMESPACE::data_format::FloatRGB, 0, offsetof(MeshVertex, position)},
                            {VSL_NAMESPACE::data_format::FloatRGB, 1, offsetof(MeshVertex, normal)},
                            {VSL_NAMESPACE::data_format::FloatRGB, 2, offsetof(MeshVertex, color)},
                            {VSL_NAMESPACE::data_format::Float, 3, offsetof(MeshVertex, material)}},
                .binding = 0,
                .size = sizeof(MeshVertex)});
            pl::PushConstants constants;
            constants.definitions = {{VSL_NAMESPACE::ShaderFlag::Vertex | VSL_NAMESPACE::ShaderFlag::Fragment, 0, pushBytes}};
            auto swapchain = target.swapchain();
            VSL_NAMESPACE::PipelineLayout nextLayout(engine._data->logical_device, pl::ShaderGroup("AHO experimental mesh", {vertex, fragment}), input,
                           constants, pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None}, pl::DepthStencil{},
                           pl::Multisample{}, pl::DynamicState{}, VSL_NAMESPACE::Viewport(swapchain), VSL_NAMESPACE::Scissor(swapchain));
            VSL_NAMESPACE::GraphicsPipeline nextPipeline(nextLayout, next);
            pipeline = std::move(nextPipeline);
            layout = std::move(nextLayout);
            pass = next;
        }
    };
    MeshRenderer::MeshRenderer(engine::GraphicalEngine& engine, RenderTarget& target, std::span<const MeshVertex> vertices,
                               const std::filesystem::path& vs, const std::filesystem::path& fs, unsigned pushBytes) {
        if (vertices.empty() || vertices.size() % 3 || vertices.size() > std::numeric_limits<unsigned>::max())
            throw std::invalid_argument("Mesh requires a nonempty triangle list fitting uint32");
        // Vulkan guarantees at least 128 bytes of push constants on every implementation.
        if (!pushBytes || pushBytes > 128 || pushBytes % 4)
            throw std::invalid_argument("Push constant size must be a multiple of 4 in [4, 128]");
        data = std::make_unique<Data>(engine, target, vertices, vs, fs, pushBytes);
    }
    MeshRenderer::~MeshRenderer() {
        if (data) {
            data->subscription = {};
            data->engine._data->command_manager.waitIdle();
        }
    }
    void MeshRenderer::record(DrawPhase& phase, std::span<const std::byte> constants) {
        if (phase.engine->_data != data->engine._data)
            throw std::invalid_argument("Mesh renderer and DrawPhase use different engines");
        auto swapchain = data->target.swapchain();
        if (phase.swapchain._data != swapchain._data || data->pass->_data != data->target.renderPass()._data)
            throw std::logic_error("Mesh renderer has not received its target update");
        if (constants.size() != data->pushBytes)
            throw std::invalid_argument("Push constant size differs from mesh pipeline layout");
        if (phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit)
            throw std::logic_error("DrawPhase has no acquired frame");
        phase << *data->pipeline << VSL_NAMESPACE::Viewport(swapchain) << VSL_NAMESPACE::Scissor(swapchain)
              << cmd::PushConstant(*data->layout, VSL_NAMESPACE::ShaderFlag::Vertex | VSL_NAMESPACE::ShaderFlag::Fragment, data->pushBytes, 0, constants.data())
              << cmd::BindVertexBuffer(data->vertices) << cmd::Draw(data->count);
    }
} // namespace AHO_NAMESPACE::x::scene
