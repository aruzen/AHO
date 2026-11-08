#include <VSL/vulkan/pipeline.hpp>
#include <VSL/vulkan/render_pass.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/shader_ops.hpp>
#include <limits>

namespace {
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages(VSL_NAMESPACE::PipelineLayoutAccessor layout);
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages(VSL_NAMESPACE::PipelineLayoutAccessor layout) {
        if (!layout.data_ || !layout.data_->info || !layout.data_->device)
            throw std::invalid_argument("Pipeline requires an initialized layout");
        const auto& shaders = layout.data_->info->shaders;
        if (shaders.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::out_of_range("Pipeline shader count exceeds uint32_t");
        std::vector<vk::PipelineShaderStageCreateInfo> result;
        result.reserve(shaders.size());
        for (const auto& shader : shaders) {
            if (!shader || !shader.use_count() || shader->device != layout.data_->device.get())
                throw std::invalid_argument("Pipeline shaders require the layout device");
            result.push_back(VSL_NAMESPACE::operation::shader_stage(*shader));
        }
        return result;
    }
} // namespace

VSL_NAMESPACE::GraphicsPipeline::GraphicsPipeline(VSL_NAMESPACE::PipelineLayoutAccessor layout, VSL_NAMESPACE::RenderPassAccessor pass, std::uint32_t subpass) {
    if (!pass.data_ || subpass >= pass.description().subpasses.size())
        throw std::out_of_range("Pipeline subpass index");
    if (!layout.data_ || !layout.data_->info || layout.data_->device != pass.data_->device)
        throw std::invalid_argument("Pipeline requires initialized layout and pass on the same device");
    const auto& outputs = pass.description().subpasses[subpass];
    if (layout.data_->info->color_blend.attachmentCount != outputs.colors.size())
        throw std::invalid_argument("Pipeline color blend count does not match subpass outputs");
    for (auto ref : outputs.colors)
        if (ref.attachment != UNUSED_ATTACHMENT && layout.data_->info->multisample.rasterizationSamples !=
                                                       static_cast<vk::SampleCountFlagBits>(pass.description().attachments[ref.attachment].samples))
            throw std::invalid_argument("Pipeline sample count does not match subpass");
    if (outputs.depth_stencil && outputs.depth_stencil->attachment != UNUSED_ATTACHMENT &&
        layout.data_->info->multisample.rasterizationSamples != static_cast<vk::SampleCountFlagBits>(
                                                                    pass.description().attachments[outputs.depth_stencil->attachment].samples))
        throw std::invalid_argument("Pipeline depth sample count does not match subpass");
    const auto stages = shader_stages(layout);
    data_ = std::shared_ptr<impl::PipelineImpl>(new impl::PipelineImpl);
    data_->device = layout.data_->device;
    data_->layout = layout.data_;
    data_->bind_point = vk::PipelineBindPoint::eGraphics;

    vk::PipelineCacheCreateInfo pipeline_info_cache;
    auto cache = data_->device->device.createPipelineCacheUnique(pipeline_info_cache);

    vk::GraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.stageCount = static_cast<std::uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &layout.data_->info->vertex_input;
    pipeline_info.pInputAssemblyState = &layout.data_->info->input_assembly;
    pipeline_info.pViewportState = &layout.data_->info->viewport;
    pipeline_info.pRasterizationState = &layout.data_->info->rasterization;
    pipeline_info.pMultisampleState = &layout.data_->info->multisample;
    if (layout.data_->info->depth_stencil)
        pipeline_info.pDepthStencilState = &layout.data_->info->depth_stencil.value();
    else
        pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &layout.data_->info->color_blend;
    pipeline_info.pDynamicState = &layout.data_->info->dynamic_state;

    pipeline_info.layout = layout.data_->pipeline_layout;
    pipeline_info.renderPass = pass.data_->render_pass;
    pipeline_info.subpass = subpass;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    const auto& result = data_->device->device.createGraphicsPipeline(*cache, pipeline_info);
    if (result.result != vk::Result::eSuccess) {
        loggingln(vk::to_string(result.result));
    }
    data_->pipeline = result.value;
}

void VSL_NAMESPACE::GraphicsPipeline::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer.retain(data_);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                                                data_->pipeline);
}

VSL_NAMESPACE::ComputePipeline::ComputePipeline(VSL_NAMESPACE::PipelineLayoutAccessor layout) {
    const auto stages = shader_stages(layout);
    if (stages.size() != 1 || stages.front().stage != vk::ShaderStageFlagBits::eCompute)
        throw std::invalid_argument("ComputePipeline requires exactly one compute shader");

    data_ = std::shared_ptr<impl::PipelineImpl>(new impl::PipelineImpl);
    data_->device = layout.data_->device;
    data_->layout = layout.data_;
    data_->bind_point = vk::PipelineBindPoint::eCompute;

    vk::PipelineCacheCreateInfo pipeline_info_cache;
    auto cache = data_->device->device.createPipelineCacheUnique(pipeline_info_cache);

    vk::ComputePipelineCreateInfo pipeline_info;
    pipeline_info.layout = layout.data_->pipeline_layout;
    pipeline_info.stage = stages.front();

    data_->pipeline = data_->device->device.createComputePipeline(*cache, pipeline_info).value;
}

void VSL_NAMESPACE::ComputePipeline::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer.retain(data_);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].bindPipeline(vk::PipelineBindPoint::eCompute,
                                                                                data_->pipeline);
}

VSL_NAMESPACE::impl::PipelineImpl::~PipelineImpl() {
    device->device.destroyPipeline(pipeline);
}
