#include "pch.h"
#include "draw.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::command::Draw::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager){
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].draw(3, 1, 0, 0);
}
