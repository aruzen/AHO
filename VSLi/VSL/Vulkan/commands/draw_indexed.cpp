#ifdef _MSC_VER
#include "pch.h"
#endif
#include "draw_indexed.h"
#include "../_pimpls.h"

VSL_NAMESPACE::command::DrawIndexedManip VSL_NAMESPACE::command::drawIndexed;

VSL_NAMESPACE::command::DrawIndexed::DrawIndexed(size_t size) : size(size) {}

void VSL_NAMESPACE::command::DrawIndexed::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager){
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].drawIndexed(size, 1, 0, 0, 0);
}

void VSL_NAMESPACE::command::DrawIndexedManip::manipulate(DefaultPhaseStreamOperator* op, CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].drawIndexed(op->vertexSize, 1, 0, 0, 0);
}
