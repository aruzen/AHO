#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/_pimpls.hpp>

VSL_NAMESPACE::command::RenderPassBegin::RenderPassBegin(RenderPass renderPass, FrameBufferAccessor frameBuffer, std::optional<__VSLDRGBColorAccessor<float>> color)
	: renderPass(renderPass), frameBuffer(frameBuffer), clear_color(color) {}

void VSL_NAMESPACE::command::RenderPassBegin::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	vk::RenderPassBeginInfo renderPassInfo;
	renderPassInfo.renderPass = renderPass._data->renderPass;
	renderPassInfo.framebuffer = frameBuffer._data->swapChainFramebuffers[buffer.getCurrentBufferIdx()];

	renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderPassInfo.renderArea.extent = frameBuffer._data->swapchain->swapChainExtent;

	vk::ClearValue clearColor = clear_color.has_value()
		? vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{ clear_color->r, clear_color->g, clear_color->b, 0.0f }))
		: vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }));
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
}


VSL_NAMESPACE::command::IDPickingRenderPassBegin::IDPickingRenderPassBegin(IDPickingRenderPass renderPass,
                                                                           FrameBufferAccessor frameBuffer,
                                                                           std::optional<__VSLDRGBColorAccessor<float>> color)
        : renderPass(renderPass), frameBuffer(frameBuffer), clear_color(color) {}

void VSL_NAMESPACE::command::IDPickingRenderPassBegin::invoke(CommandPool pool, CommandBuffer buffer,
                                                              CommandManager manager) {
    vk::RenderPassBeginInfo renderPassInfo;
    renderPassInfo.renderPass = renderPass._data->renderPass;
    renderPassInfo.framebuffer = frameBuffer._data->swapChainFramebuffers[buffer.getCurrentBufferIdx()];

    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = frameBuffer._data->swapchain->swapChainExtent;

    std::array<vk::ClearValue, 2> clearColors{
            clear_color.has_value()
            ? vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{clear_color->r, clear_color->g, clear_color->b, 0.0f}))
            : vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f})),
            vk::ClearColorValue(std::array<uint32_t,4>{0, 0, 0, 0})
    };
    renderPassInfo.clearValueCount = clearColors.size();
    renderPassInfo.pClearValues = clearColors.data();

    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].beginRenderPass(renderPassInfo,
                                                                               vk::SubpassContents::eInline);
}
