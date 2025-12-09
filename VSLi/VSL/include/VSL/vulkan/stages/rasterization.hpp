#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
	struct Rasterization {
		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}
