#include "../drawer/renderer2d_impl.hpp"
#include <VSL/vulkan/commands/bind_vertex_buffers.hpp>
#include <VSL/vulkan/commands/draw.hpp>
#include <VSL/vulkan/commands/push_content.hpp>
#include <VSL/vulkan/scissor.hpp>
#include <VSL/vulkan/viewport.hpp>
#include <algorithm>

namespace AHO_NAMESPACE::x::command {
    Draw2DCommand::Draw2DCommand(std::shared_ptr<Data> data) : _data(std::move(data)) {}

    VSL_NAMESPACE::PipelineAccessor Draw2DCommand::getPipeline() const { return _data->pipeline->pipeline; }

    void Draw2DCommand::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) const {
        if (!_data || !pool._data || !buffer._data || !manager._data || manager.getDevice()._data != _data->pipeline->device._data)
            throw std::invalid_argument("Draw command requires its renderer device");
        auto submission = _data->submission.lock();
        if (!submission || VSL_NAMESPACE::Completion{submission}.status() != VSL_NAMESPACE::Completion::Status::Pending ||
            manager._data != _data->manager.lock() || pool._data != manager.getPool()._data || buffer._data != manager.getBuffer()._data)
            throw std::logic_error("Draw snapshot must be recorded in the phase that prepared it");
        if (!_data->batches.empty()) {
            auto& pending = _data->slot->submissions;
            if (std::ranges::none_of(pending, [&](const auto& weak) { return weak.lock() == submission; }))
                pending.push_back(submission);
        }
        auto& pipeline = *_data->pipeline;
        pipeline.pipeline.invoke(pool, buffer, manager);
        VSL_NAMESPACE::Viewport(0, 0, static_cast<float>(_data->width), static_cast<float>(_data->height)).invoke(pool, buffer, manager);
        VSL_NAMESPACE::Scissor(0, 0, static_cast<float>(_data->width), static_cast<float>(_data->height)).invoke(pool, buffer, manager);
        const std::array<float, 4> transform{2.f / _data->width, 2.f / _data->height, -1, -1};
        VSL_NAMESPACE::command::PushConstant(pipeline.layout, VSL_NAMESPACE::ShaderFlag::Vertex, 16, 0, transform.data()).invoke(pool, buffer, manager);
        for (std::size_t i = 0; i < _data->batches.size(); ++i) {
            VSL_NAMESPACE::command::BindVertexBuffer(_data->slot->uploads[i]->buffer).invoke(pool, buffer, manager);
            VSL_NAMESPACE::command::Draw(_data->batches[i].vertex_count).invoke(pool, buffer, manager);
        }
    }
} // namespace AHO_NAMESPACE::x::command
