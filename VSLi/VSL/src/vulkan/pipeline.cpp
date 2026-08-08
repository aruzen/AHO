#include <VSL/vulkan/pipeline.hpp>
#include <VSL/vulkan/render_pass.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <source_location>

vsl::GraphicsPipeline::GraphicsPipeline(vsl::PipelineLayoutAccessor layout, vsl::RenderPassAccessor pass) {
    if (layout._data == nullptr || layout._data->device == nullptr || !layout._data->pipelineLayout ||
        pass._data == nullptr || !pass._data->renderPass) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Pipeline",
                "GraphicsPipeline creation requires a valid layout and render pass.",
                std::source_location::current());
    }
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
        if (cache)
            _data->device->device.destroyPipelineCache(cache);
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Pipeline",
                "Failed to create graphics pipeline: " + vk::to_string(result.result),
                std::source_location::current());
    }
    _data->pipeline = result.value;
    if (cache)
        _data->device->device.destroyPipelineCache(cache);
}

void vsl::GraphicsPipeline::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                                            _data->pipeline);
}

vsl::ComputePipeline::ComputePipeline(vsl::PipelineLayoutAccessor layout) {
    if (layout._data == nullptr || layout._data->device == nullptr || !layout._data->pipelineLayout ||
        layout._data->info == nullptr || layout._data->info->shaderStages.empty()) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Pipeline",
                "ComputePipeline creation requires a valid layout and compute shader stage.",
                std::source_location::current());
    }

    _data = std::shared_ptr<_impl::Pipeline_impl>(new _impl::Pipeline_impl);
    _data->device = layout._data->device;
    _data->layout = layout._data;

    vk::PipelineCacheCreateInfo pipelineInfoCache;
    // pipelineInfoCache.flags = vk::PipelineCacheCreateFlagBits::
    auto cache = _data->device->device.createPipelineCache(pipelineInfoCache);

    vk::ComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.layout = layout._data->pipelineLayout;
    pipelineInfo.stage = layout._data->info->shaderStages[0];

    const auto result = _data->device->device.createComputePipeline(cache, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        if (cache)
            _data->device->device.destroyPipelineCache(cache);
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Pipeline",
                "Failed to create compute pipeline: " + vk::to_string(result.result),
                std::source_location::current());
    }
    _data->pipeline = result.value;
    if (cache)
        _data->device->device.destroyPipelineCache(cache);
}

void vsl::ComputePipeline::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eCompute,
                                                                            _data->pipeline);
}

VSL_NAMESPACE::_impl::Pipeline_impl::~Pipeline_impl() {
    if (device != nullptr && pipeline)
        device->device.destroyPipeline(pipeline);
}
