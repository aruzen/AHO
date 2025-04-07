#ifdef _MSC_VER
#include "pch.h"
#endif
#include "render_pass_end.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::command::RenderPassEnd::invoke(CommandPool pool, CommandBuffer buffer)
{
	buffer._data->commandBuffers[0].endRenderPass();
}
