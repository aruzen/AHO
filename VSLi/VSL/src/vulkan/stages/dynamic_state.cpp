#include <VSL/operation/graphics_pipeline_state_access.hpp>
#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::operation::detail::GraphicsPipelineStateAccess;
}

void VSL_NAMESPACE::pipeline_layout::DynamicState::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    injection(pl.edit_state());
}

void VSL_NAMESPACE::pipeline_layout::DynamicState::injection(VSL_NAMESPACE::GraphicsPipelineStateEditRef state) {
    auto& info = GraphicsPipelineStateAccess::Edit(state);
    info.enabled_dynamic_states.assign(
        {vk::DynamicState::eScissor,
         vk::DynamicState::eViewport});
}
