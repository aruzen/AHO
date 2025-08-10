#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../pipeline_layout.h"
#include "../descriptor.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
	struct ResourceBinding {
        ResourceBinding(std::vector<ResourceBindingLayout> sets);

        std::vector<ResourceBindingLayout> resourceBindingSets;

		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}