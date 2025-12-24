#include <VSL/vulkan/commands/bind_pipe_line.hpp>
#include <VSL/vulkan/_pimpls.hpp>

VSL_NAMESPACE::command::BindPipeline::BindPipeline(PipelineAccessor pipeline): pipeline(pipeline) {}

void VSL_NAMESPACE::command::BindPipeline::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline._data->pipeline);
}

