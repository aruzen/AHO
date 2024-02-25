#include "pch.h"
#include "vertex_input.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::VertexInput::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	info.vertexInput.vertexBindingDescriptionCount = 0;
	info.vertexInput.pVertexBindingDescriptions = nullptr; // Optional
	info.vertexInput.vertexAttributeDescriptionCount = 0;
	info.vertexInput.pVertexAttributeDescriptions = nullptr; // Optional
}
