#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::DynamicState::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = *pl.data_->info;
    info.enabled_dynamic_states.assign(
        {vk::DynamicState::eScissor,
         vk::DynamicState::eViewport});
}
