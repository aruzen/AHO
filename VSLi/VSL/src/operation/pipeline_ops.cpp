#include <VSL/operation/pipeline_ops.hpp>
#include <limits>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::operation {
    namespace {
        thread_local const GraphicsPipelineNativeFactory* NATIVE_FACTORY = nullptr;
        void validate_layout(const impl::LogicalDeviceImpl& device, const impl::PipelineLayoutImpl* layout);
        vk::Pipeline checked_pipeline(vk::Device device, vk::Result result, vk::Pipeline pipeline);
        std::vector<vk::PipelineShaderStageCreateInfo> graphics_stages(const impl::LogicalDeviceImpl& device, std::span<impl::ShaderImpl* const> shaders);
        void validate_pass(const impl::LogicalDeviceImpl& device, const GraphicsPipelineInput& input);

        void validate_layout(const impl::LogicalDeviceImpl& device, const impl::PipelineLayoutImpl* layout) {
            validate_pipeline_device(device);
            if (!layout || !layout->pipeline_layout || layout->device != &device)
                throw std::invalid_argument("Pipeline requires a live layout on its device");
        }
        vk::Pipeline checked_pipeline(vk::Device device, vk::Result result, vk::Pipeline pipeline) {
            if (result != vk::Result::eSuccess || !pipeline) {
                if (pipeline)
                    device.destroyPipeline(pipeline);
                throw std::runtime_error("Pipeline creation failed: " + vk::to_string(result));
            }
            return pipeline;
        }
        std::vector<vk::PipelineShaderStageCreateInfo> graphics_stages(const impl::LogicalDeviceImpl& device, std::span<impl::ShaderImpl* const> shaders) {
            if (shaders.empty() || shaders.size() > UINT32_MAX)
                throw std::invalid_argument("Graphics pipeline needs a representable nonempty shader sequence");
            constexpr auto allowed = static_cast<std::uint32_t>(vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eTaskEXT | vk::ShaderStageFlagBits::eMeshEXT);
            std::uint32_t present = 0;
            std::vector<vk::PipelineShaderStageCreateInfo> stages;
            stages.reserve(shaders.size());
            for (const auto* shader : shaders) {
                if (!shader || shader->device != &device)
                    throw std::invalid_argument("Pipeline shaders require the layout device");
                const auto stage = shader_stage(*shader);
                const auto bits = static_cast<std::uint32_t>(stage.stage);
                if ((bits & ~allowed) || (bits & present))
                    throw std::invalid_argument("Graphics pipeline requires distinct graphics shader stages");
                present |= bits;
                stages.push_back(stage);
            }
            constexpr auto vertex_or_mesh = static_cast<std::uint32_t>(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eMeshEXT);
            if (!(present & vertex_or_mesh))
                throw std::invalid_argument("Graphics pipeline requires a vertex or mesh shader");
            return stages;
        }
        void validate_pass(const impl::LogicalDeviceImpl& device, const GraphicsPipelineInput& input) {
            if (!input.pass || !input.pass->render_pass || input.pass->device != &device || !input.pass->description || !input.state)
                throw std::invalid_argument("Graphics pipeline requires a live pass and state on its device");
            const auto& description = *input.pass->description;
            if (input.subpass >= description.subpasses.size())
                throw std::out_of_range("Pipeline subpass index");
            const auto& outputs = description.subpasses[input.subpass];
            if (input.state->blend_attachments.size() != outputs.colors.size())
                throw std::invalid_argument("Pipeline color blend count does not match subpass outputs");
            const auto check_samples = [&](AttachmentReference reference) {
                if (reference.attachment != UNUSED_ATTACHMENT && input.state->multisample.rasterizationSamples !=
                                                                     static_cast<vk::SampleCountFlagBits>(description.attachments[reference.attachment].samples))
                    throw std::invalid_argument("Pipeline sample count does not match subpass");
            };
            for (const auto reference : outputs.colors)
                check_samples(reference);
            if (outputs.depth_stencil)
                check_samples(*outputs.depth_stencil);
        }
    } // namespace
    ScopedGraphicsPipelineNativeFactory::ScopedGraphicsPipelineNativeFactory(GraphicsPipelineNativeFactory factory) noexcept
        : _factory(factory), _previous(std::exchange(NATIVE_FACTORY, &_factory)) {}
    ScopedGraphicsPipelineNativeFactory::~ScopedGraphicsPipelineNativeFactory() {
        NATIVE_FACTORY = _previous;
    }
    void initialize_pipeline(impl::PipelineImpl& target, impl::LogicalDeviceImpl& device, const GraphicsPipelineInput& input) {
        validate_layout(device, input.layout);
        validate_pass(device, input);
        const auto stages = graphics_stages(device, input.shaders);
        const PipelineScratch scratch(*input.state);
        const vk::GraphicsPipelineCreateInfo info{
            .stageCount = static_cast<std::uint32_t>(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &scratch.vertex_input,
            .pInputAssemblyState = &input.state->input_assembly,
            .pViewportState = &scratch.viewport,
            .pRasterizationState = &input.state->rasterization,
            .pMultisampleState = &input.state->multisample,
            .pDepthStencilState = input.state->depth_stencil ? &*input.state->depth_stencil : nullptr,
            .pColorBlendState = &scratch.color_blend,
            .pDynamicState = &scratch.dynamic_state,
            .layout = input.layout->pipeline_layout,
            .renderPass = input.pass->render_pass,
            .subpass = input.subpass,
            .basePipelineIndex = -1};
        const auto factory = NATIVE_FACTORY ? *NATIVE_FACTORY : GraphicsPipelineNativeFactory{};
        const auto cache = device.device.createPipelineCacheUnique({});
        vk::Pipeline pipeline;
        const auto result = factory.create ? factory.create(factory.context, device.device, *cache, info, &pipeline)
                                           : device.device.createGraphicsPipelines(*cache, 1, &info, nullptr, &pipeline);
        target.pipeline = checked_pipeline(device.device, result, pipeline);
        target.native_device = device.device;
        target.device = &device;
        target.layout = input.layout;
        target.bind_point = vk::PipelineBindPoint::eGraphics;
    }
    void initialize_pipeline(impl::PipelineImpl& target, impl::LogicalDeviceImpl& device, const ComputePipelineInput& input) {
        validate_layout(device, input.layout);
        if (!input.shader || input.shader->device != &device)
            throw std::invalid_argument("Compute shader requires the layout device");
        const auto stage = shader_stage(*input.shader);
        if (stage.stage != vk::ShaderStageFlagBits::eCompute)
            throw std::invalid_argument("Compute pipeline requires one compute shader");
        const vk::ComputePipelineCreateInfo info{.stage = stage, .layout = input.layout->pipeline_layout, .basePipelineIndex = -1};
        const auto cache = device.device.createPipelineCacheUnique({});
        vk::Pipeline pipeline;
        const auto result = device.device.createComputePipelines(*cache, 1, &info, nullptr, &pipeline);
        target.pipeline = checked_pipeline(device.device, result, pipeline);
        target.native_device = device.device;
        target.device = &device;
        target.layout = input.layout;
        target.bind_point = vk::PipelineBindPoint::eCompute;
    }
    void destroy_pipeline(impl::PipelineImpl& target) noexcept {
        if (target.pipeline)
            target.native_device.destroyPipeline(target.pipeline);
        target.pipeline = nullptr;
    }
} // namespace VSL_NAMESPACE::operation
