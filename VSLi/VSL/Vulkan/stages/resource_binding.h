#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../pipeline_layout.h"
#include "../descriptor.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
	struct ResourceBinding {
        ResourceBinding(std::vector<graphic_resource::BindingLayout> sets);

        std::vector<graphic_resource::BindingLayout> resourceBindingSets;

		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}