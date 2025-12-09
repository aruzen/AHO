#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif
#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::DynamicState::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;
    info.enabledDynamicStates.assign(
            {vk::DynamicState::eViewportWithCount,
             vk::DynamicState::eScissorWithCount});
}
