#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::DynamicState::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    injection(pl.edit_state());
}

void VSL_NAMESPACE::pipeline_layout::DynamicState::injection(VSL_NAMESPACE::GraphicsPipelineStateEditRef state) {
    auto& info = state.get();
    info.enabled_dynamic_states.assign(
        {vk::DynamicState::eScissor,
         vk::DynamicState::eViewport});
}
