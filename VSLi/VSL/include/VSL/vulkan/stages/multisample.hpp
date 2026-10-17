#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"
#include "../buffer_and_image_accessor.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
	struct Multisample {
        SampleCount samples = SampleCount::One;
		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}
