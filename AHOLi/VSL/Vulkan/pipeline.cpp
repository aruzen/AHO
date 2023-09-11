#include "pch.h"
#include "pipeline.h"

#include "_pimpls.h"


VSL_NAMESPACE::Pipeline::Pipeline(PipelineLayoutAccesor layout)
{
	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
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
}
