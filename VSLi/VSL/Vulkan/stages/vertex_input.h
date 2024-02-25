#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../pipeline_layout.h"

namespace VSL_NAMESPACE::pipeline_layout {
	struct VertexInput {
		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}
