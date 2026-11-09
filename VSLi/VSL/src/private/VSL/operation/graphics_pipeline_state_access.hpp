#pragma once
#include <VSL/operation/graphics_pipeline_state.hpp>

namespace VSL_NAMESPACE::operation::detail {
    struct GraphicsPipelineStateAccess {
        static const impl::GraphicsPipelineStateData& Read(const GraphicsPipelineState& state);
        static const impl::GraphicsPipelineStateData& Read(const GraphicsPipelineState&& state) = delete;
        static impl::GraphicsPipelineStateData& Edit(GraphicsPipelineStateEditRef state);
        static GraphicsPipelineStateEditRef Borrow(impl::GraphicsPipelineStateData& state) noexcept { return GraphicsPipelineStateEditRef(&state); }
    };
} // namespace VSL_NAMESPACE::operation::detail
