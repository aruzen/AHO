#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>

namespace VSL_NAMESPACE::command {
	struct Draw : public __Command {
		Draw(size_t size = 0);

		size_t size;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

	struct DrawManip : public __Command, __VertexSizeRequire {
        std::optional<size_t> size;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;

        void setVertexSize(std::optional<size_t> vertexSize) override;
    };

	extern DrawManip draw;
}