#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include <optional>

namespace VSL_NAMESPACE::command {
	struct DrawIndexed : public __Command {
        DrawIndexed(size_t size = 0);

		size_t size;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

	struct DrawIndexedManip : public DrawIndexed, public __VertexSizeRequire{
        void setVertexSize(std::optional<size_t> vertexSize) override;
    };

	extern DrawIndexedManip drawIndexed;
}