#ifdef _MSC_VER
#include "pch.h"
#endif
#include "dynamic_state.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::DynamicState::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;
    info.enabledDynamicStates.assign(
            {vk::DynamicState::eViewportWithCount,
             vk::DynamicState::eScissorWithCount});
}
