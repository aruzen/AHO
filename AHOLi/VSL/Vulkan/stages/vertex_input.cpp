#include "vertex_input.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::VertexInput::injection(VSL_NAMESPACE::_impl::CreateInfo& info)
{
	info.vertexInput.vertexBindingDescriptionCount = 0;
	info.vertexInput.pVertexBindingDescriptions = nullptr; // Optional
	info.vertexInput.vertexAttributeDescriptionCount = 0;
	info.vertexInput.pVertexAttributeDescriptions = nullptr; // Optional
}
