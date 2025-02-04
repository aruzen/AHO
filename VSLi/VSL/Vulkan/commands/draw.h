#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include <optional>

namespace VSL_NAMESPACE::command {
	struct Draw : public __Command {
		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}