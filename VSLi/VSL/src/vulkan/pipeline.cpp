#include <VSL/vulkan/pipeline.hpp>
#include <VSL/lifetime/pipeline_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    namespace {
        const impl::PipelineRecipe& checked_recipe(const PipelineLayoutAccessor& layout);
        std::vector<impl::ShaderImpl*> shader_sources(std::span<const ShaderAccessor> shaders);
        const impl::PipelineRecipe& checked_recipe(const PipelineLayoutAccessor& layout) {
            (void)layout.get_device();
            if (!layout.has_recipe())
                throw std::invalid_argument("Pipeline requires a layout recipe or explicit state and shaders");
            const auto& recipe = layout.recipe();
            if (!operation::pipeline_layout_matches(*layout.data_, recipe.binding_layouts, recipe.push_ranges))
                throw std::invalid_argument("Pipeline recipe has changed since native layout construction");
            return recipe;
        }
        std::vector<impl::ShaderImpl*> shader_sources(std::span<const ShaderAccessor> shaders) {
            std::vector<impl::ShaderImpl*> result;
            result.reserve(shaders.size());
            for (const auto& shader : shaders) {
                if (!shader.data_ || !shader.data_.use_count())
                    throw std::invalid_argument("Pipeline shader source requires an owner");
                result.push_back(shader.data_.get());
            }
            return result;
        }
    } // namespace
    LogicalDeviceAccessor PipelineAccessor::get_device() const {
        if (!data_ || !data_.use_count() || !data_->pipeline || !data_->device || !data_->layout)
            throw std::invalid_argument("Pipeline requires a live owner");
        return {data_->device->shared_from_this()};
    }
    lifetime::OwnerRef PipelineAccessor::owner_ref() const {
        if (!data_ || !data_.use_count())
            return {};
        return lifetime::detail::owned_ref(data_, data_->owner_node);
    }
    GraphicsPipeline::GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, std::uint32_t subpass) {
        if (!pass.data_ || !pass.data_.use_count() || subpass >= pass.description().subpasses.size())
            throw std::out_of_range("Pipeline subpass index");
        (void)pass.get_device();
        const auto& recipe = checked_recipe(layout);
        const operation::GraphicsPipelineInput input{layout.data_.get(), pass.data_.get(), &recipe.graphics_state, recipe.shaders, subpass};
        data_ = lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input);
    }
    GraphicsPipeline::GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, const GraphicsPipelineState& state,
                                       std::span<const ShaderAccessor> shaders, std::uint32_t subpass) {
        (void)layout.get_device();
        (void)pass.get_device();
        const auto sources = shader_sources(shaders);
        const operation::GraphicsPipelineInput input{layout.data_.get(), pass.data_.get(), &state.definition(), sources, subpass};
        data_ = lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input);
    }
    GraphicsPipeline::GraphicsPipeline(PipelineAccessor source) : PipelineAccessor(std::move(source)) {
        (void)get_device();
        if (data_->bind_point != vk::PipelineBindPoint::eGraphics)
            throw std::invalid_argument("Graphics pipeline requires the graphics bind point");
    }
    void GraphicsPipeline::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        buffer.retain(data_);
        buffer.data_->command_buffers[buffer.get_current_buffer_idx()].bindPipeline(vk::PipelineBindPoint::eGraphics, data_->pipeline);
    }
    ComputePipeline::ComputePipeline(PipelineLayoutAccessor layout) {
        const auto& recipe = checked_recipe(layout);
        if (recipe.shaders.size() != 1)
            throw std::invalid_argument("Compute pipeline requires exactly one compute shader");
        const operation::ComputePipelineInput input{layout.data_.get(), recipe.shaders.front()};
        data_ = lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input);
    }
    ComputePipeline::ComputePipeline(PipelineLayoutAccessor layout, ShaderAccessor shader) {
        (void)layout.get_device();
        const std::array<ShaderAccessor, 1> inputs{std::move(shader)};
        const auto sources = shader_sources(inputs);
        const operation::ComputePipelineInput input{layout.data_.get(), sources.front()};
        data_ = lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input);
    }
    ComputePipeline::ComputePipeline(PipelineAccessor source) : PipelineAccessor(std::move(source)) {
        (void)get_device();
        if (data_->bind_point != vk::PipelineBindPoint::eCompute)
            throw std::invalid_argument("Compute pipeline requires the compute bind point");
    }
    void ComputePipeline::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        buffer.retain(data_);
        buffer.data_->command_buffers[buffer.get_current_buffer_idx()].bindPipeline(vk::PipelineBindPoint::eCompute, data_->pipeline);
    }
} // namespace VSL_NAMESPACE
