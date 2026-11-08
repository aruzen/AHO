#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
    struct DynamicState {
        void injection(GraphicsPipelineStateEditRef state);
        void injection(PipelineLayoutEditAccessor pl);
    };
} // namespace VSL_NAMESPACE::pipeline_layout
