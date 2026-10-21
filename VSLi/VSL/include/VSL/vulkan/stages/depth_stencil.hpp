#pragma once
#include "../pipeline_layout.hpp"
namespace VSL_NAMESPACE::pipeline_layout {
    enum class CompareOp {
        NEVER = 0,
        LESS = 1,
        EQUAL = 2,
        LESS_OR_EQUAL = 3,
        GREATER = 4,
        NOT_EQUAL = 5,
        GREATER_OR_EQUAL = 6,
        ALWAYS = 7
    };
    enum class StencilOp {
        KEEP = 0,
        ZERO = 1,
        REPLACE = 2,
        INCREMENT_CLAMP = 3,
        DECREMENT_CLAMP = 4,
        INVERT = 5,
        INCREMENT_WRAP = 6,
        DECREMENT_WRAP = 7
    };
    struct StencilFace {
        StencilOp fail = StencilOp::KEEP, pass = StencilOp::KEEP, depth_fail = StencilOp::KEEP;
        CompareOp compare = CompareOp::ALWAYS;
        std::uint32_t compare_mask = UINT32_MAX, write_mask = UINT32_MAX, reference = 0;
    };
    struct DepthStencil {
        bool depth_test = true, depth_write = true;
        CompareOp depth_compare = CompareOp::LESS;
        bool stencil_test = false;
        StencilFace front, back;
        void injection(PipelineLayoutEditAccessor pl);
    };
} // namespace VSL_NAMESPACE::pipeline_layout
