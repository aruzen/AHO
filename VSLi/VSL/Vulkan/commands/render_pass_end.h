#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

namespace VSL_NAMESPACE::command {
	struct RenderPassEnd : public __Command {
		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}

