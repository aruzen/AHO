#pragma once

#include "../../define.hpp"
#include "../../vulkan/pv.hpp"
#include "../../vulkan/pipeline_layout.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
    struct IDPicking {
        void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
    };
}
