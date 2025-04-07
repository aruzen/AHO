#ifdef _MSC_VER
#include "pch.h"
#endif
#include "render_pass_begin.h"
#include "../_pimpls.h"

VSL_NAMESPACE::command::RenderPassBegin::RenderPassBegin(RenderPass renderPass, FrameBufferAccessor frameBuffer, std::optional<__VSLDRGBColorAccessor<float>> color)
	: renderPass(renderPass), frameBuffer(frameBuffer), target_idx(frameBuffer._data->currentIndex), clear_color(color) {}

VSL_NAMESPACE::command::RenderPassBegin::RenderPassBegin(RenderPass renderPass, FrameBufferAccessor frameBuffer, size_t imageIdx, std::optional<__VSLDRGBColorAccessor<float>> color)
	: renderPass(renderPass), frameBuffer(frameBuffer), target_idx(imageIdx), clear_color(color) {}


void VSL_NAMESPACE::command::RenderPassBegin::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	vk::RenderPassBeginInfo renderPassInfo;
	renderPassInfo.renderPass = renderPass._data->renderPass;
	renderPassInfo.framebuffer = frameBuffer._data->swapChainFramebuffers[target_idx];

	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = frameBuffer._data->swapchain->swapChainExtent;

	vk::ClearValue clearColor = clear_color.has_value()
		? vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{ clear_color->r, clear_color->g, clear_color->b, 0.0f }))
		: vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }));
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
}
