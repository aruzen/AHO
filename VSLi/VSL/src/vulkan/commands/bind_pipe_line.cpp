#include <VSL/vulkan/commands/bind_pipe_line.hpp>
#include <VSL/vulkan/_pimpls.hpp>

VSL_NAMESPACE::command::BindPipeline::BindPipeline(PipelineAccessor pipeline) : pipeline(pipeline) {}

VSL_NAMESPACE::PipelineAccessor VSL_NAMESPACE::command::BindPipeline::getPipeline() { return pipeline; }

void VSL_NAMESPACE::command::BindPipeline::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.retain(pipeline._data);
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(pipeline._data->bindPoint, pipeline._data->pipeline);
}
