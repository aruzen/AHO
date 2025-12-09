#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../pipeline.hpp"

namespace VSL_NAMESPACE::command {
	struct BindPipeline : __Command {
		BindPipeline(PipelineAccessor pipeline);

        PipelineAccessor pipeline;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}