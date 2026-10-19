#include <VSL/vulkan/pipeline.hpp>
#include <VSL/vulkan/render_pass.hpp>

#include <VSL/vulkan/_pimpls.hpp>

vsl::GraphicsPipeline::GraphicsPipeline(vsl::PipelineLayoutAccessor layout, vsl::RenderPassAccessor pass, std::uint32_t subpass) {
    if (!pass._data || subpass >= pass.description().subpasses.size()) throw std::out_of_range("Pipeline subpass index");
    if (!layout._data || !layout._data->info || layout._data->device != pass._data->device)
        throw std::invalid_argument("Pipeline requires editable layout and pass on the same device");
    const auto &outputs = pass.description().subpasses[subpass];
    if (layout._data->info->colorBlend.attachmentCount != outputs.colors.size())
        throw std::invalid_argument("Pipeline color blend count does not match subpass outputs");
    for (auto ref : outputs.colors)
        if (ref.attachment != UnusedAttachment && layout._data->info->multisample.rasterizationSamples !=
            static_cast<vk::SampleCountFlagBits>(pass.description().attachments[ref.attachment].samples))
            throw std::invalid_argument("Pipeline sample count does not match subpass");
    if (outputs.depthStencil && outputs.depthStencil->attachment != UnusedAttachment &&
        layout._data->info->multisample.rasterizationSamples != static_cast<vk::SampleCountFlagBits>(
            pass.description().attachments[outputs.depthStencil->attachment].samples))
        throw std::invalid_argument("Pipeline depth sample count does not match subpass");
    _data = std::shared_ptr<_impl::Pipeline_impl>(new _impl::Pipeline_impl);
    _data->device = layout._data->device;
    _data->layout = layout._data;

    vk::PipelineCacheCreateInfo pipelineInfoCache;
    auto cache = _data->device->device.createPipelineCacheUnique(pipelineInfoCache);

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
    pipelineInfo.subpass = subpass;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    const auto& result = _data->device->device.createGraphicsPipeline(*cache, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
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
    auto cache = _data->device->device.createPipelineCacheUnique(pipelineInfoCache);

    vk::ComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.layout = layout._data->pipelineLayout;
    pipelineInfo.stage = layout._data->info->shaderStages[0];

    _data->pipeline = _data->device->device.createComputePipeline(*cache, pipelineInfo).value;
}

void vsl::ComputePipeline::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindPipeline(vk::PipelineBindPoint::eCompute,
                                                                            _data->pipeline);
}

VSL_NAMESPACE::_impl::Pipeline_impl::~Pipeline_impl() {
    device->device.destroyPipeline(pipeline);
}
