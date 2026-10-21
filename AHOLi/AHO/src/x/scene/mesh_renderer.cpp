#include <AHO/x/scene/mesh_renderer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <VSL/vsl.hpp>
#include <optional>
#include <limits>
#include <stdexcept>

namespace AHO_NAMESPACE::x::scene {
    namespace pl = VSL_NAMESPACE::pipeline_layout;
    namespace cmd = VSL_NAMESPACE::command;
    using Depth = VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::DepthStencilAttachment>;
    using Buffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::VertexBuffer, VSL_NAMESPACE::MemoryProperty::HostVisible | VSL_NAMESPACE::MemoryProperty::HostCoherent>;
    struct MeshRenderer::Data {
        engine::GraphicalEngine& engine;
        Window window;
        Buffer vertices;
        unsigned count, pushBytes;
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Vertex> vertex;
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Fragment> fragment;
        std::optional<VSL_NAMESPACE::SwapchainAccessor> swapchain;
        std::optional<VSL_NAMESPACE::RenderPass> pass;
        std::vector<Depth> depths;
        std::vector<VSL_NAMESPACE::FrameBuffer> frames;
        std::optional<VSL_NAMESPACE::PipelineLayout> layout;
        std::optional<VSL_NAMESPACE::GraphicsPipeline> pipeline;
        std::array<unsigned, 2> size{};
        Data(engine::GraphicalEngine& e, std::span<const MeshVertex> source, const std::filesystem::path& vs, const std::filesystem::path& fs, unsigned bytes)
            : engine(e), window(e.getWindow()), vertices(e._data->logical_device, source.size_bytes()), count(unsigned(source.size())), pushBytes(bytes),
              vertex(e._data->logical_device, vs), fragment(e._data->logical_device, fs) {
            if (!vertices.copy(source.data(), source.size()))
                throw std::runtime_error("Mesh vertex upload failed");
        }
        void prepare() {
            if (swapchain && swapchain->_data == window._data2->swapchain._data)
                return;
            engine._data->command_manager.waitIdle();
            frames.clear();
            depths.clear();
            swapchain = window._data2->swapchain;
            auto frameSize = window.frame_size();
            size = {unsigned(frameSize.value.x.value), unsigned(frameSize.value.y.value)};
            const bool changedFormat = !pass || pass->description().attachments.front().format.type() != swapchain->getFormat().type();
            if (changedFormat) {
                pipeline.reset();
                layout.reset();
                pass.reset();
                VSL_NAMESPACE::RenderPassDescription description;
                description.addColor({.format = swapchain->getFormat(), .finalLayout = VSL_NAMESPACE::ImageLayout::Present});
                description.setDepthStencil({.format = VSL_NAMESPACE::data_format::Depth32Float, .store = VSL_NAMESPACE::StoreOp::DontCare, .finalLayout = VSL_NAMESPACE::ImageLayout::DepthStencilAttachmentOptimal});
                pass.emplace(engine._data->logical_device, std::move(description));
            }
            for (auto image : swapchain->images) {
                depths.emplace_back(engine._data->logical_device, size[0], size[1], VSL_NAMESPACE::data_format::Depth32Float);
                frames.emplace_back(*pass, size[0], size[1], std::vector<VSL_NAMESPACE::ImageAccessor>{image, depths.back()});
            }
            // Extent is dynamic. Retain compatible pipelines across resize.
            if (!changedFormat)
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
            layout.emplace(engine._data->logical_device, pl::ShaderGroup("AHO experimental mesh", {vertex, fragment}), input,
                           constants, pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None}, pl::DepthStencil{},
                           pl::Multisample{}, pl::DynamicState{}, VSL_NAMESPACE::Viewport(*swapchain), VSL_NAMESPACE::Scissor(*swapchain));
            pipeline.emplace(*layout, *pass);
        }
    };
    MeshRenderer::MeshRenderer(engine::GraphicalEngine& engine, std::span<const MeshVertex> vertices,
                               const std::filesystem::path& vs, const std::filesystem::path& fs, unsigned pushBytes) {
        if (vertices.empty() || vertices.size() % 3 || vertices.size() > std::numeric_limits<unsigned>::max())
            throw std::invalid_argument("Mesh requires a nonempty triangle list fitting uint32");
        // Vulkan guarantees at least 128 bytes of push constants on every implementation.
        if (!pushBytes || pushBytes > 128 || pushBytes % 4)
            throw std::invalid_argument("Push constant size must be a multiple of 4 in [4, 128]");
        data = std::make_unique<Data>(engine, vertices, vs, fs, pushBytes);
        data->prepare();
    }
    MeshRenderer::~MeshRenderer() {
        if (data)
            data->engine._data->command_manager.waitIdle();
    }
    void MeshRenderer::prepare() { data->prepare(); }
    std::array<unsigned, 2> MeshRenderer::extent() const { return data->size; }
    VSL_NAMESPACE::RenderPassAccessor MeshRenderer::renderPass() const { return *data->pass; }
    VSL_NAMESPACE::SwapchainAccessor MeshRenderer::swapchain() const { return *data->swapchain; }
    void MeshRenderer::record(DrawPhase& phase, std::span<const std::byte> constants) {
        if (phase.engine->_data != data->engine._data)
            throw std::invalid_argument("Mesh renderer and DrawPhase use different engines");
        if (phase.swapchain._data != data->swapchain->_data)
            throw std::logic_error("Prepare mesh renderer after the window's resize");
        if (constants.size() != data->pushBytes)
            throw std::invalid_argument("Push constant size differs from mesh pipeline layout");
        if (phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit)
            throw std::logic_error("DrawPhase has no acquired frame");
        phase << cmd::RenderPassBegin(data->frames.at(phase.getImageIndex()), {VSL_NAMESPACE::ClearColor{.1f, .2f, .23f, 1}, VSL_NAMESPACE::ClearDepthStencil{1, 0}});
        phase << *data->pipeline << VSL_NAMESPACE::Viewport(*data->swapchain) << VSL_NAMESPACE::Scissor(*data->swapchain)
              << cmd::PushConstant(*data->layout, VSL_NAMESPACE::ShaderFlag::Vertex | VSL_NAMESPACE::ShaderFlag::Fragment, data->pushBytes, 0, constants.data())
              << cmd::BindVertexBuffer(data->vertices) << cmd::Draw(data->count);
    }
} // namespace AHO_NAMESPACE::x::scene
