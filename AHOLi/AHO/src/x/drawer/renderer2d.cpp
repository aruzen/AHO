#include "renderer2d_impl.hpp"
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
    namespace _impl {
        Upload::Upload(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t bytes) : buffer(device, bytes), capacity(bytes) {}

        bool Slot::available() {
            std::erase_if(submissions, [](const auto& weak) {
                auto submission = weak.lock();
                if (!submission)
                    return true;
                VSL_NAMESPACE::Completion completion{submission};
                return completion.status() == VSL_NAMESPACE::Completion::Status::Cancelled || completion.ready();
            });
            return submissions.empty();
        }

        Pipeline::Pipeline(VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::RenderPassAccessor pass,
                           const std::filesystem::path& directory, std::uint32_t subpass)
            : device(device), layout(makeLayout(device, directory)), pipeline(layout, pass, subpass) {}

        VSL_NAMESPACE::PipelineLayout Pipeline::makeLayout(VSL_NAMESPACE::LogicalDeviceAccessor device, const std::filesystem::path& directory) {
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
                    pl::ColorBlend(pl::ColorBlendAttachment{.destinationAlpha = pl::BlendFactor::OneMinusSrcAlpha}),
                    pl::InputAssembly{}, pl::Rasterization(pl::CullingMode::None),
                    pl::Multisample{}, pl::DynamicState{},
                    VSL_NAMESPACE::Viewport(0, 0, 1, 1), VSL_NAMESPACE::Scissor(0, 0, 1, 1)};
        }
    } // namespace _impl

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
        _data = std::make_shared<Data>();
        _data->pipeline = std::make_shared<_impl::Pipeline>(device, pass, shaders, subpass);
        _data->limit = maxSnapshots;
    }

    std::optional<command::Draw2DCommand> Renderer2D::draw(VSL_NAMESPACE::ComputePhase& phase, const DrawList& list, std::uint32_t width, std::uint32_t height) {
        if (phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit || !phase.completion() || !phase.manager._data ||
            phase.manager.getDevice()._data != _data->pipeline->device._data)
            throw std::invalid_argument("Draw snapshot requires a recording phase on the renderer device");
        if (!width || !height || width > 16777216 || height > 16777216)
            throw std::invalid_argument("Draw extent must be positive and exactly representable as float");
        for (const auto& batch : list.batches())
            if (batch.texture.value)
                throw std::invalid_argument("Solid Renderer2D does not accept texture IDs");
        auto snapshot = std::make_shared<command::Draw2DCommand::Data>();
        snapshot->submission = phase.completion()._data;
        snapshot->manager = phase.manager._data;
        snapshot->pipeline = _data->pipeline;
        snapshot->width = width;
        snapshot->height = height;
        snapshot->batches.assign(list.batches().begin(), list.batches().end());
        if (list.empty())
            return command::Draw2DCommand(std::move(snapshot));
        std::shared_ptr<_impl::Slot> slot;
        for (const auto& candidate : _data->slots)
            if (candidate.use_count() == 1 && candidate->available()) {
                slot = candidate;
                break;
            }
        if (!slot) {
            if (_data->slots.size() >= _data->limit)
                return std::nullopt;
            slot = std::make_shared<_impl::Slot>();
            _data->slots.push_back(slot);
        }
        for (std::size_t i = 0; i < snapshot->batches.size(); ++i) {
            const auto& batch = snapshot->batches[i];
            if (static_cast<std::uintmax_t>(batch.vertexCount) * sizeof(DrawVertex) > std::numeric_limits<std::size_t>::max())
                throw std::length_error("Draw upload byte count overflow");
            const auto bytes = static_cast<std::size_t>(batch.vertexCount) * sizeof(DrawVertex);
            if (slot->uploads.size() <= i)
                slot->uploads.push_back(std::make_unique<_impl::Upload>(_data->pipeline->device, bytes));
            else if (slot->uploads[i]->capacity < bytes)
                slot->uploads[i] = std::make_unique<_impl::Upload>(_data->pipeline->device, bytes);
            auto mapped = slot->uploads[i]->buffer.data(bytes);
            std::memcpy(mapped.data, list.vertices().data() + batch.firstVertex, bytes);
        }
        snapshot->slot = std::move(slot);
        return command::Draw2DCommand(std::move(snapshot));
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
        for (const auto& slot : _data->slots)
            for (const auto& upload : slot->uploads)
                result += upload->capacity;
        return result;
    }
} // namespace AHO_NAMESPACE::x
