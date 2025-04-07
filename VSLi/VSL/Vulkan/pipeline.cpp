#ifdef _MSC_VER
#include "pch.h"
#endif
#include "pipeline.h"
#include "render_pass.h"

#include "_pimpls.h"


VSL_NAMESPACE::Pipeline::Pipeline(PipelineLayoutAccessor layout, RenderPass pass)
{
	_data = std::shared_ptr<_impl::Pipeline_impl>(new _impl::Pipeline_impl);
	_data->device = layout._data->device;
	_data->layout = layout._data;

	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.stageCount = (uint32_t)layout._data->info->shaderStages.size();
	pipelineInfo.pStages = layout._data->info->shaderStages.data();
	pipelineInfo.pVertexInputState = &layout._data->info->vertexInput;
	pipelineInfo.pInputAssemblyState = &layout._data->info->inputAssembly;
	pipelineInfo.pViewportState = &layout._data->info->_viewport;
	pipelineInfo.pRasterizationState = &layout._data->info->rasterization;
	pipelineInfo.pMultisampleState = &layout._data->info->multisample;
	if (layout._data->info->depthStencil)
		pipelineInfo.pDepthStencilState = &layout._data->info->depthStencil.value();
	else
		pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &layout._data->info->colorBlend;
	pipelineInfo.pDynamicState = &layout._data->info->dynamicState;

	pipelineInfo.layout = layout._data->pipelineLayout;
	pipelineInfo.renderPass = pass._data->renderPass;
	pipelineInfo.subpass = 0;

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	vk::PipelineCacheCreateInfo pipelineInfoCache;
	// pipelineInfoCache.flags = vk::PipelineCacheCreateFlagBits::
	auto cache = _data->device->device.createPipelineCache(pipelineInfoCache);

	_data->pipeline = _data->device->device.createGraphicsPipeline(cache, pipelineInfo).value;
	/*
	if (vkCreateGraphicsPipelines(
		layout._data->device->device, VK_NULL_HANDLE, 1,
		reinterpret_cast<const VkGraphicsPipelineCreateInfo*>(&pipelineInfo), nullptr,
		reinterpret_cast<VkPipeline*>(&_data->pipeline)) != VK_SUCCESS) {
		// throw std::runtime_error("failed to create graphics pipeline!");
	}*/
}


VSL_NAMESPACE::_impl::Pipeline_impl::~Pipeline_impl() {
	device->device.destroyPipeline(pipeline);
}

void VSL_NAMESPACE::Pipeline::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eGraphics, _data->pipeline);
}
