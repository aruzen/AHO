#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../pipeline_layout.h"

namespace VSL_NAMESPACE::pipeline_layout {
	struct DepthStencil {
		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}