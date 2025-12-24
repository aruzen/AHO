#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif
#include <VSL/vulkan/viewport.hpp>

#include <VSL/vulkan/_pimpls.hpp>

VSL_NAMESPACE::Viewport::Viewport() {}

VSL_NAMESPACE::Viewport::Viewport(float x, float y, float width, float height) : x(x), y(y), width(width), height(height) {}

VSL_NAMESPACE::Viewport::Viewport(__VSLD2RectangleAccessor<float> rect) : x(rect.x), y(rect.y), width(rect.width), height(rect.height) {}

VSL_NAMESPACE::Viewport::Viewport(SwapchainAccessor swapchain) : x(0), y(0)
, height(swapchain._data->swapChainExtent.height)
, width(swapchain._data->swapChainExtent.width) {}

void VSL_NAMESPACE::Viewport::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl) {
	auto& info = *pl._data->info;

	vk::Viewport viewport;
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	info.viewports.push_back(viewport);
}

void VSL_NAMESPACE::Viewport::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
	vk::Viewport viewport;
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].setViewport(0, viewport);
}
