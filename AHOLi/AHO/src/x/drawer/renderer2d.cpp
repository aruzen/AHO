#include <AHO/x/drawer/renderer2d.hpp>
#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/commands/bind_vertex_buffers.hpp>
#include <VSL/vulkan/commands/draw.hpp>
#include <VSL/vulkan/commands/push_content.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/stages/multisample.hpp>
#include <VSL/vulkan/stages/push_contents.hpp>
#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/stages/vertex_input.hpp>
#include <VSL/vulkan/scissor.hpp>
#include <VSL/vulkan/viewport.hpp>
#include <cstring>
#include <algorithm>

namespace AHO_NAMESPACE::x {
    namespace {
        using VertexBuffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::VertexBuffer,
                                                   VSL_NAMESPACE::MemoryProperty::HostVisible | VSL_NAMESPACE::MemoryProperty::HostCoherent>;
        struct Upload {
            VertexBuffer buffer;
            std::size_t capacity;
            Upload(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t bytes) : buffer(device, bytes), capacity(bytes) {}
        };
        struct Slot {
            std::vector<std::unique_ptr<Upload>> uploads;
            std::vector<std::weak_ptr<VSL_NAMESPACE::_impl::Submission_impl>> submissions;
            bool available() {
                std::erase_if(submissions, [](const auto& weak) {
                    auto submission = weak.lock();
                    if (!submission)
                        return true;
                    VSL_NAMESPACE::Completion completion{submission};
                    return completion.status() == VSL_NAMESPACE::Completion::Status::Cancelled || completion.ready();
                });
                return submissions.empty();
            }
        };
        struct Pipeline {
            VSL_NAMESPACE::LogicalDeviceAccessor device;
            VSL_NAMESPACE::PipelineLayout layout;
            VSL_NAMESPACE::GraphicsPipeline pipeline;
            Pipeline(VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::RenderPassAccessor pass,
                     const std::filesystem::path& directory, std::uint32_t subpass)
                : device(device), layout(makeLayout(device, directory)), pipeline(layout, pass, subpass) {}
            static VSL_NAMESPACE::PipelineLayout makeLayout(VSL_NAMESPACE::LogicalDeviceAccessor device, const std::filesystem::path& directory) {
                namespace pl = VSL_NAMESPACE::pipeline_layout;
                VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Vertex> vertex(device, directory / "draw2d.vert.spv");
                VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Fragment> fragment(device, directory / "draw2d.frag.spv");
                pl::VertexInputShapeDefinition shape{
                    .layouts = {{VSL_NAMESPACE::data_format::FloatRG, 0, offsetof(DrawVertex, position)},
                                {VSL_NAMESPACE::data_format::FloatRG, 1, offsetof(DrawVertex, uv)},
                                {VSL_NAMESPACE::data_format::FloatRGBA, 2, offsetof(DrawVertex, color)}},
                    .binding = 0,
                    .size = sizeof(DrawVertex)};
                return {device, pl::ShaderGroup("aho-draw2d", {vertex, fragment}), pl::VertexInput({shape}),
                        pl::PushConstants(VSL_NAMESPACE::ShaderFlag::Vertex, {{VSL_NAMESPACE::ShaderFlag::Vertex, 0, 16}}),
                        pl::ColorBlend(pl::ColorBlendAttachment{.destinationAlpha = pl::BlendFactor::OneMinusSrcAlpha}), pl::InputAssembly{}, pl::Rasterization(pl::CullingMode::None), pl::Multisample{}, pl::DynamicState{},
                        VSL_NAMESPACE::Viewport(0, 0, 1, 1), VSL_NAMESPACE::Scissor(0, 0, 1, 1)};
            }
        };
    } // namespace
    struct Renderer2D::Data {
        std::shared_ptr<Pipeline> pipeline;
        std::vector<std::shared_ptr<Slot>> slots;
        std::size_t limit;
    };
    struct Draw2DCommand::Data {
        std::shared_ptr<Pipeline> pipeline;
        std::shared_ptr<Slot> slot;
        std::vector<DrawBatch> batches;
        std::uint32_t width, height;
        std::weak_ptr<VSL_NAMESPACE::_impl::Submission_impl> submission;
        std::weak_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> manager;
    };
    Renderer2D::Renderer2D(VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::RenderPassAccessor pass,
                           const std::filesystem::path& shaders, std::size_t maxSnapshots, std::uint32_t subpass) {
        if (!device._data || !pass._data || !maxSnapshots)
            throw std::invalid_argument("Renderer2D requires device, pass and a nonzero snapshot limit");
        const auto& description = pass.description();
        if (subpass >= description.subpasses.size())
            throw std::out_of_range("Renderer2D subpass is out of range");
        const auto& target = description.subpasses[subpass];
        if (target.colors.size() != 1 || target.depthStencil || target.colors[0].attachment == VSL_NAMESPACE::UnusedAttachment)
            throw std::invalid_argument("Renderer2D requires one color attachment and no depth/stencil attachment");
        const auto& attachment = description.attachments.at(target.colors[0].attachment);
        using namespace VSL_NAMESPACE::data_format;
        if (attachment.samples != VSL_NAMESPACE::SampleCount::One ||
            (attachment.format != UnsignedNormalized8RGBA && attachment.format != UnsignedNormalized8BGRA &&
             attachment.format != Srgb8RGBA && attachment.format != Srgb8BGRA))
            throw std::invalid_argument("Renderer2D requires a single-sample RGBA8/BGRA8 UNORM or sRGB target");
        data_ = std::make_shared<Data>();
        data_->pipeline = std::make_shared<Pipeline>(device, pass, shaders, subpass);
        data_->limit = maxSnapshots;
    }
    std::optional<Draw2DCommand> Renderer2D::draw(VSL_NAMESPACE::ComputePhase& phase, const DrawList& list, std::uint32_t width, std::uint32_t height) {
        if (phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit || !phase.completion() || !phase.manager._data ||
            phase.manager.getDevice()._data != data_->pipeline->device._data)
            throw std::invalid_argument("Draw snapshot requires a recording phase on the renderer device");
        if (!width || !height || width > 16777216 || height > 16777216)
            throw std::invalid_argument("Draw extent must be positive and exactly representable as float");
        for (const auto& batch : list.batches())
            if (batch.texture.value)
                throw std::invalid_argument("Solid Renderer2D does not accept texture IDs");
        auto snapshot = std::make_shared<Draw2DCommand::Data>();
        snapshot->submission = phase.completion()._data;
        snapshot->manager = phase.manager._data;
        snapshot->pipeline = data_->pipeline;
        snapshot->width = width;
        snapshot->height = height;
        snapshot->batches.assign(list.batches().begin(), list.batches().end());
        if (list.empty())
            return Draw2DCommand(std::move(snapshot));
        std::shared_ptr<Slot> slot;
        for (const auto& candidate : data_->slots)
            if (candidate.use_count() == 1 && candidate->available()) {
                slot = candidate;
                break;
            }
        if (!slot) {
            if (data_->slots.size() >= data_->limit)
                return std::nullopt;
            slot = std::make_shared<Slot>();
            data_->slots.push_back(slot);
        }
        for (std::size_t i = 0; i < snapshot->batches.size(); ++i) {
            const auto& batch = snapshot->batches[i];
            if (static_cast<std::uintmax_t>(batch.vertexCount) * sizeof(DrawVertex) > std::numeric_limits<std::size_t>::max())
                throw std::length_error("Draw upload byte count overflow");
            const auto bytes = static_cast<std::size_t>(batch.vertexCount) * sizeof(DrawVertex);
            if (slot->uploads.size() <= i)
                slot->uploads.push_back(std::make_unique<Upload>(data_->pipeline->device, bytes));
            else if (slot->uploads[i]->capacity < bytes)
                slot->uploads[i] = std::make_unique<Upload>(data_->pipeline->device, bytes);
            auto mapped = slot->uploads[i]->buffer.data(bytes);
            std::memcpy(mapped.data, list.vertices().data() + batch.firstVertex, bytes);
        }
        snapshot->slot = std::move(slot);
        return Draw2DCommand(std::move(snapshot));
    }
    bool Renderer2D::record(VSL_NAMESPACE::ComputePhase& phase, const DrawList& list, std::uint32_t width, std::uint32_t height) {
        auto command = draw(phase, list, width, height);
        if (!command)
            return false;
        phase << *command;
        return true;
    }
    std::size_t Renderer2D::capacity() const noexcept {
        std::size_t result = 0;
        for (const auto& slot : data_->slots)
            for (const auto& upload : slot->uploads)
                result += upload->capacity;
        return result;
    }
    VSL_NAMESPACE::PipelineAccessor Draw2DCommand::getPipeline() const { return data_->pipeline->pipeline; }
    void Draw2DCommand::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) const {
        if (!data_ || !pool._data || !buffer._data || !manager._data || manager.getDevice()._data != data_->pipeline->device._data)
            throw std::invalid_argument("Draw command requires its renderer device");
        auto submission = data_->submission.lock();
        if (!submission || VSL_NAMESPACE::Completion{submission}.status() != VSL_NAMESPACE::Completion::Status::Pending ||
            manager._data != data_->manager.lock() || pool._data != manager.getPool()._data || buffer._data != manager.getBuffer()._data)
            throw std::logic_error("Draw snapshot must be recorded in the phase that prepared it");
        if (!data_->batches.empty()) {
            auto& pending = data_->slot->submissions;
            if (std::ranges::none_of(pending, [&](const auto& weak) { return weak.lock() == submission; }))
                pending.push_back(submission);
        }
        auto& pipeline = *data_->pipeline;
        pipeline.pipeline.invoke(pool, buffer, manager);
        VSL_NAMESPACE::Viewport(0, 0, static_cast<float>(data_->width), static_cast<float>(data_->height)).invoke(pool, buffer, manager);
        VSL_NAMESPACE::Scissor(0, 0, static_cast<float>(data_->width), static_cast<float>(data_->height)).invoke(pool, buffer, manager);
        const std::array<float, 4> transform{2.f / data_->width, 2.f / data_->height, -1, -1};
        VSL_NAMESPACE::command::PushConstant(pipeline.layout, VSL_NAMESPACE::ShaderFlag::Vertex, 16, 0, transform.data()).invoke(pool, buffer, manager);
        for (std::size_t i = 0; i < data_->batches.size(); ++i) {
            VSL_NAMESPACE::command::BindVertexBuffer(data_->slot->uploads[i]->buffer).invoke(pool, buffer, manager);
            VSL_NAMESPACE::command::Draw(data_->batches[i].vertexCount).invoke(pool, buffer, manager);
        }
    }
} // namespace AHO_NAMESPACE::x
