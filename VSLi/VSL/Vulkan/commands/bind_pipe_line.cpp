#ifdef _MSC_VER
#include "pch.h"
#endif
#include "bind_pipe_line.h"
#include "../_pimpls.h"

VSL_NAMESPACE::command::BindPipeline::BindPipeline(PipelineAccessor pipeline): pipeline(pipeline) {}

void VSL_NAMESPACE::command::BindPipeline::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline._data->pipeline);
}

