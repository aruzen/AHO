#include "pch.h"
#include "draw.h"
#include "../_pimpls.h"

VSL_NAMESPACE::command::DrawManip VSL_NAMESPACE::command::draw;

VSL_NAMESPACE::command::Draw::Draw(size_t size) : size(size) {}

void VSL_NAMESPACE::command::Draw::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager){
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].draw(size, 1, 0, 0);
}

void VSL_NAMESPACE::command::DrawManip::manipulate(DefaultPhaseStreamOperator* op, CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].draw(op->vertexSize, 1, 0, 0);
}
