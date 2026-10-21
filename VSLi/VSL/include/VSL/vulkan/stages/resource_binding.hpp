#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"
#include "../descriptor.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
    struct ResourceBinding {
        ResourceBinding(std::vector<graphic_resource::BindingLayout> sets);

        std::vector<graphic_resource::BindingLayout> resource_binding_sets;

        void injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl);
    };
} // namespace VSL_NAMESPACE::pipeline_layout
