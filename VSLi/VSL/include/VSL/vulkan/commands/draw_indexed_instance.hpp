#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include <optional>

namespace VSL_NAMESPACE::command {
	struct DrawInstance : public __Command {
        DrawInstance(size_t index_size, size_t instance_size);
        DrawInstance(size_t instance_size);

        size_t index_size = 0, instance_size;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

    /* FIXME
	struct DrawInstanceManip : public DrawInstance, public __VertexSizeRequire{
        void setVertexSize(std::optional<size_t> vertexSize) override;
    };

	extern DrawInstanceManip drawInstance;
     */
}