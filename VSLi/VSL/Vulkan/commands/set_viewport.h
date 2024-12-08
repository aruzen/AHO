#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include "../viewport.h"

namespace VSL_NAMESPACE::command {
	struct SetViewport : public __Command {
		SetViewport(Viewport port);

		Viewport port;

		void invoke(CommandPool pool, CommandBuffer buffer);
	};
}

