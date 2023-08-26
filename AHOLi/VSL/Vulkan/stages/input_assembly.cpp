#include "input_assembly.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::_impl::CreateInfo& info)
{
	info.inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	info.inputAssembly.primitiveRestartEnable = false;
}
