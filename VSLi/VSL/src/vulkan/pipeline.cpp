#include <VSL/lifetime/owner_validation.hpp>
#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/shader_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/vulkan/pipeline.hpp>
#include <VSL/lifetime/pipeline_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ShaderAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE {
    PipelineIdentity PipelineAccessor::identity() const {
        return lifetime::detail::IdentityAccess::From<PipelineIdentity>(_owner.get());
    }
    namespace {
        const impl::PipelineRecipe& checked_recipe(const PipelineLayoutAccessor& layout);
        std::vector<impl::ShaderImpl*> shader_sources(std::span<const ShaderAccessor> shaders);
        const impl::PipelineRecipe& checked_recipe(const PipelineLayoutAccessor& layout) {
            (void)layout.get_device();
            if (!layout.has_recipe())
                throw std::invalid_argument("Pipeline requires a layout recipe or explicit state and shaders");
            const auto& recipe = layout.recipe();
            if (!operation::pipeline_layout_matches(*GraphicsAccess::Owned(layout), recipe.binding_layouts, recipe.push_ranges))
                throw std::invalid_argument("Pipeline recipe has changed since native layout construction");
            return recipe;
        }
        std::vector<impl::ShaderImpl*> shader_sources(std::span<const ShaderAccessor> shaders) {
            std::vector<impl::ShaderImpl*> result;
            result.reserve(shaders.size());
            for (const auto& shader : shaders) {
                if (!ShaderAccess::Owned(shader) || !ShaderAccess::Owned(shader).use_count())
                    throw std::invalid_argument("Pipeline shader source requires an owner");
                result.push_back(ShaderAccess::Owned(shader).get());
            }
            return result;
        }
    } // namespace
    LogicalDeviceAccessor PipelineAccessor::get_device() const {
        if (!_owner || !_owner.use_count() || !_owner->pipeline || !_owner->device || !_owner->layout)
            throw std::invalid_argument("Pipeline requires a live owner");
        return lifetime::device_from_child(_owner);
    }
    lifetime::OwnerRef PipelineAccessor::owner_ref() const {
        lifetime::detail::require_live_owner(_owner);
        return _owner;
    }
    GraphicsPipeline::GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, std::uint32_t subpass) {
        if (!GraphicsAccess::Owned(pass) || !GraphicsAccess::Owned(pass).use_count() || subpass >= pass.description().subpasses.size())
            throw std::out_of_range("Pipeline subpass index");
        (void)pass.get_device();
        const auto& recipe = checked_recipe(layout);
        std::vector<ShaderAccessor> kept;
        kept.reserve(recipe.shaders.size());
        for (size_t i = 0; i < recipe.shaders.size(); ++i)
            kept.push_back(ShaderAccess::FromRecipe(recipe, i));
        const auto sources = shader_sources(kept);
        const auto state = recipe.graphics_state;
        const operation::GraphicsPipelineInput input{GraphicsAccess::Owned(layout).get(), GraphicsAccess::Owned(pass).get(), &state, sources, subpass};
        GraphicsAccess::Owned(*this) = GraphicsAccess::Owned(GraphicsAccess::FromShared(lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input)));
    }
    GraphicsPipeline::GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, const GraphicsPipelineState& state,
                                       std::span<const ShaderAccessor> shaders, std::uint32_t subpass) {
        (void)layout.get_device();
        (void)pass.get_device();
        const std::vector<ShaderAccessor> kept(shaders.begin(), shaders.end());
        const auto sources = shader_sources(kept);
        const GraphicsPipelineState snapshot(state);
        const operation::GraphicsPipelineInput input{GraphicsAccess::Owned(layout).get(), GraphicsAccess::Owned(pass).get(), &snapshot.definition(), sources, subpass};
        GraphicsAccess::Owned(*this) = GraphicsAccess::Owned(GraphicsAccess::FromShared(lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input)));
    }
    GraphicsPipeline::GraphicsPipeline(PipelineAccessor source) : _value(std::move(source)) {
        (void)get_device();
        if (GraphicsAccess::Owned(*this)->bind_point != vk::PipelineBindPoint::eGraphics)
            throw std::invalid_argument("Graphics pipeline requires the graphics bind point");
    }
    void GraphicsPipeline::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        const auto owner = GraphicsAccess::Owned(*this);
        if (!owner || !owner->pipeline)
            throw std::logic_error("Graphics pipeline is empty");
        const NativeRecordingContext recording(std::move(buffer));
        recording.retain(owner, ResourceRetention::SUBMISSION);
        vk::CommandBuffer(recording.native_command_buffer()).bindPipeline(vk::PipelineBindPoint::eGraphics, owner->pipeline);
    }
    ComputePipeline::ComputePipeline(PipelineLayoutAccessor layout) {
        const auto& recipe = checked_recipe(layout);
        if (recipe.shaders.size() != 1)
            throw std::invalid_argument("Compute pipeline requires exactly one compute shader");
        const auto shader = ShaderAccess::FromRecipe(recipe, 0);
        const operation::ComputePipelineInput input{GraphicsAccess::Owned(layout).get(), ShaderAccess::Owned(shader).get()};
        GraphicsAccess::Owned(*this) = GraphicsAccess::Owned(GraphicsAccess::FromShared(lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input)));
    }
    ComputePipeline::ComputePipeline(PipelineLayoutAccessor layout, ShaderAccessor shader) {
        (void)layout.get_device();
        const std::array<ShaderAccessor, 1> inputs{std::move(shader)};
        const auto sources = shader_sources(inputs);
        const operation::ComputePipelineInput input{GraphicsAccess::Owned(layout).get(), sources.front()};
        GraphicsAccess::Owned(*this) = GraphicsAccess::Owned(GraphicsAccess::FromShared(lifetime::make_heap_pipeline(layout.get_device(), layout.owner_ref(), input)));
    }
    ComputePipeline::ComputePipeline(PipelineAccessor source) : _value(std::move(source)) {
        (void)get_device();
        if (GraphicsAccess::Owned(*this)->bind_point != vk::PipelineBindPoint::eCompute)
            throw std::invalid_argument("Compute pipeline requires the compute bind point");
    }
    void ComputePipeline::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        const auto owner = GraphicsAccess::Owned(*this);
        if (!owner || !owner->pipeline)
            throw std::logic_error("Compute pipeline is empty");
        const NativeRecordingContext recording(std::move(buffer));
        recording.retain(owner, ResourceRetention::SUBMISSION);
        vk::CommandBuffer(recording.native_command_buffer()).bindPipeline(vk::PipelineBindPoint::eCompute, owner->pipeline);
    }
} // namespace VSL_NAMESPACE

VSL_NAMESPACE::PipelineAccessor VSL_NAMESPACE::lifetime::detail::GraphicsAccess::FromShared(std::shared_ptr<impl::PipelineImpl> owner) {
    const auto* node = owner ? owner->owner_node : nullptr;
    return FromOwned(owned_ref(std::move(owner), node));
}
