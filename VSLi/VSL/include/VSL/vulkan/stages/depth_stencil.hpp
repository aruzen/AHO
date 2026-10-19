#pragma once
#include "../pipeline_layout.hpp"
namespace VSL_NAMESPACE::pipeline_layout {
    enum class CompareOp {
        Never = 0,
        Less = 1,
        Equal = 2,
        LessOrEqual = 3,
        Greater = 4,
        NotEqual = 5,
        GreaterOrEqual = 6,
        Always = 7
    };
    enum class StencilOp {
        Keep = 0,
        Zero = 1,
        Replace = 2,
        IncrementClamp = 3,
        DecrementClamp = 4,
        Invert = 5,
        IncrementWrap = 6,
        DecrementWrap = 7
    };
    struct StencilFace {
        StencilOp fail = StencilOp::Keep, pass = StencilOp::Keep, depthFail = StencilOp::Keep;
        CompareOp compare = CompareOp::Always;
        std::uint32_t compareMask = UINT32_MAX, writeMask = UINT32_MAX, reference = 0;
    };
    struct DepthStencil {
        bool depthTest = true, depthWrite = true;
        CompareOp depthCompare = CompareOp::Less;
        bool stencilTest = false;
        StencilFace front, back;
        void injection(PipelineLayoutAccessor pl);
    };
} // namespace VSL_NAMESPACE::pipeline_layout
