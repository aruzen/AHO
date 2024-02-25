#include "pch.h"
#include "input_assembly.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	info.inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	info.inputAssembly.primitiveRestartEnable = false;
}
