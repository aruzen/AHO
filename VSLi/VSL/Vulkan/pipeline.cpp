#ifdef _MSC_VER
#include "pch.h"
#endif

#include "pipeline.h"
#include "render_pass.h"

#include "_pimpls.h"

vsl::GraphicsPipeline::GraphicsPipeline(vsl::PipelineLayoutAccessor layout, vsl::RenderPass pass) {
    _data = std::shared_ptr<_impl::Pipeline_impl>(new _impl::Pipeline_impl);
    _data->device = layout._data->device;
    _data->layout = layout._data;

    vk::PipelineCacheCreateInfo pipelineInfoCache;
    // pipelineInfoCache.flags = vk::PipelineCacheCreateFlagBits::
    auto cache = _data->device->device.createPipelineCache(pipelineInfoCache);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.stageCount = (uint32_t) layout._data->info->shaderStages.size();
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

    const auto& result = _data->device->device.createGraphicsPipeline(cache, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        // FIXME
        loggingln(vk::to_string(result.result));
    }
    _data->pipeline = result.value;
}

void vsl::GraphicsPipeline::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                                            _data->pipeline);
}

vsl::ComputePipeline::ComputePipeline(vsl::PipelineLayoutAccessor layout) {

    _data = std::shared_ptr<_impl::Pipeline_impl>(new _impl::Pipeline_impl);
    _data->device = layout._data->device;
    _data->layout = layout._data;

    vk::PipelineCacheCreateInfo pipelineInfoCache;
    // pipelineInfoCache.flags = vk::PipelineCacheCreateFlagBits::
    auto cache = _data->device->device.createPipelineCache(pipelineInfoCache);

    vk::ComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.layout = layout._data->pipelineLayout;
    pipelineInfo.stage = layout._data->info->shaderStages[0];

    _data->pipeline = _data->device->device.createComputePipeline(cache, pipelineInfo).value;
}

void vsl::ComputePipeline::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eCompute,
                                                                            _data->pipeline);
}

VSL_NAMESPACE::_impl::Pipeline_impl::~Pipeline_impl() {
    device->device.destroyPipeline(pipeline);
}
