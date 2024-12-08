#include "pch.h"
#include "bind_pipe_line.h"
#include "../_pimpls.h"

VSL_NAMESPACE::command::BindPipeline::BindPipeline(Pipeline pipeline): pipeline(pipeline) {}

void VSL_NAMESPACE::command::BindPipeline::invoke(CommandPool pool, CommandBuffer buffer)
{
	buffer._data->commandBuffers[0].bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline._data->pipeline);
}

