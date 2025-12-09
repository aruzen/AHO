#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

namespace VSL_NAMESPACE::command {
	struct RenderPassEnd : public __Command {
		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}

