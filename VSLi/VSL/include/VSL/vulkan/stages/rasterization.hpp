#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
    enum class CullingMode {
        None = 0,
        Front = 1,
        Back = 2,
        FrontAndBack = 3,
    };

    enum class FrontFace {
        CounterClockwise = 0,
        Clockwise = 1
    };

    struct Rasterization {
        explicit Rasterization(
            CullingMode culling_mode = CullingMode::Back,
            FrontFace front_face = FrontFace::CounterClockwise);

        void injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl);

        CullingMode culling_mode;
        FrontFace front_face;
    };
} // namespace VSL_NAMESPACE::pipeline_layout
