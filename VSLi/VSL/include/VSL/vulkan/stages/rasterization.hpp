#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
    enum class CullingMode {
        NONE = 0,
        FRONT = 1,
        BACK = 2,
        FRONT_AND_BACK = 3,
    };

    enum class FrontFace {
        COUNTER_CLOCKWISE = 0,
        CLOCKWISE = 1
    };

    struct Rasterization {
        explicit Rasterization(
            CullingMode culling_mode = CullingMode::BACK,
            FrontFace front_face = FrontFace::COUNTER_CLOCKWISE);

        void injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl);

        CullingMode culling_mode;
        FrontFace front_face;
    };
} // namespace VSL_NAMESPACE::pipeline_layout
