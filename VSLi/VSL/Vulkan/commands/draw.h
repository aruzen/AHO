#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include <optional>

namespace VSL_NAMESPACE::command {
	struct Draw : public __Command {
		Draw(size_t size = 0);

		size_t size;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

	struct DrawManip : public __Manipulator {
		void manipulate(DefaultPhaseStreamOperator* op, CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
	};

	extern DrawManip draw;
}