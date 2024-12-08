#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include "../pipeline.h"

namespace VSL_NAMESPACE::command {
	struct BindPipeline : __Command {
		BindPipeline(Pipeline pipeline);

		Pipeline pipeline;

		void invoke(CommandPool pool, CommandBuffer buffer);
	};
}