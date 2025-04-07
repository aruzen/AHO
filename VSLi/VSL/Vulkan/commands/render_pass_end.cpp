#ifdef _MSC_VER
#include "pch.h"
#endif
#include "render_pass_end.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::command::RenderPassEnd::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].endRenderPass();
}
