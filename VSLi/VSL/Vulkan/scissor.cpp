#include "pch.h"
#include "scissor.h"

#include "_pimpls.h"

VSL_NAMESPACE::Scissor::Scissor() {}

VSL_NAMESPACE::Scissor::Scissor(float x, float y, float width, float height) : x(x), y(y), width(width), height(height) {}

VSL_NAMESPACE::Scissor::Scissor(__VSLD2RectangleAccessor<float> rect) : x(rect.x), y(rect.y), width(rect.width), height(rect.height) {}

VSL_NAMESPACE::Scissor::Scissor(SwapchainAccessor swapchain) : x(0), y(0)
, height(swapchain._data->swapChainExtent.height)
, width(swapchain._data->swapChainExtent.width) {}

void VSL_NAMESPACE::Scissor::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl) {
	auto& info = *pl._data->info;

	vk::Rect2D scissor;
	scissor.offset = { x, y};
	scissor.extent = { width, height };
	info.scissors.push_back(scissor);
}

void VSL_NAMESPACE::Scissor::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
	vk::Rect2D scissor;
	scissor.offset = { x, y };
	scissor.extent = { width, height };

	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].setScissorWithCount({ scissor });
}
