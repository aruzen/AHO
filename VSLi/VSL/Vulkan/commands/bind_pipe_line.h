#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include "../pipeline.h"

namespace VSL_NAMESPACE::command {
	struct BindPipeline : __Command {
		BindPipeline(PipelineAccessor pipeline);

        PipelineAccessor pipeline;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}