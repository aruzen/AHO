#include <VSL/lifetime/pipeline_recipe.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/pipeline_storage.hpp>
#include <VSL/lifetime/shader_access.hpp>
#include <VSL/lifetime/descriptor_access.hpp>

VSL_NAMESPACE::ShaderAccessor VSL_NAMESPACE::lifetime::detail::ShaderAccess::FromRecipe(const impl::PipelineRecipe& recipe, size_t index) {
    return FromOwned(recipe.sources.at(recipe.shader_sources.at(index)).alias(recipe.shaders.at(index)));
}

namespace VSL_NAMESPACE::lifetime {
    namespace {
        constexpr typed::ResourceId LAYOUT{0}, PIPELINE{1};
        constexpr std::array REQUESTS{
            typed::ResourceRequest{LAYOUT, storage::ResourceKind::PIPELINE_LAYOUT},
            typed::ResourceRequest{PIPELINE, storage::ResourceKind::PIPELINE}};
        constexpr typed::ResourcePlan<2> PLAN(REQUESTS);
        PipelineLayoutInit layout_input(const PipelineRecipeSnapshot& snapshot);
        void validate_target(LogicalDeviceAccessor device, const RenderPassAccessor& pass, std::uint32_t subpass);
        std::vector<PipelineShaderSource> shader_inputs(const impl::PipelineRecipe& recipe);
        PipelineLayoutInit layout_input(const PipelineRecipeSnapshot& snapshot) {
            const auto& recipe = snapshot.definition();
            if (recipe.pending_layouts)
                throw std::logic_error("Pipeline descriptor layouts have not been materialized");
            PipelineLayoutInit result;
            result.push_ranges = recipe.push_ranges;
            for (auto layout : snapshot.layout_sources())
                result.layouts.emplace_back(std::move(layout));
            return result;
        }
        std::vector<PipelineShaderSource> shader_inputs(const impl::PipelineRecipe& recipe) {
            std::vector<PipelineShaderSource> result;
            result.reserve(recipe.shaders.size());
            for (size_t i = 0; i < recipe.shaders.size(); ++i)
                result.emplace_back(detail::ShaderAccess::FromRecipe(recipe, i));
            return result;
        }
        void validate_target(LogicalDeviceAccessor device, const RenderPassAccessor& pass, std::uint32_t subpass) {
            if (!pass.data_ || !pass.data_.use_count() || subpass >= pass.description().subpasses.size())
                throw std::out_of_range("Pipeline subpass index");
            if (pass.get_device() != device)
                throw std::invalid_argument("Pipeline render pass requires the pipeline device");
        }
    } // namespace

    PipelineRecipeSnapshot::PipelineRecipeSnapshot(LogicalDeviceAccessor device, std::shared_ptr<const impl::PipelineRecipe> recipe)
        : _device(std::move(device)), _recipe(std::move(recipe)) {}
    const impl::PipelineRecipe& PipelineRecipeSnapshot::definition() const& {
        if (!_recipe)
            throw std::logic_error("Pipeline recipe snapshot is empty");
        return *_recipe;
    }
    const std::optional<std::vector<DescriptorLayoutCreateInfo>>& PipelineRecipeSnapshot::pending_layouts() const& {
        return definition().pending_layouts;
    }
    PipelineRecipeSnapshot PipelineRecipeSnapshot::with_layouts(std::span<const graphic_resource::BindingLayout> layouts) const {
        auto next = std::make_shared<impl::PipelineRecipe>(definition());
        replace_recipe_layouts(*next, _device, layouts);
        return {_device, std::move(next)};
    }
    std::vector<graphic_resource::BindingLayout> PipelineRecipeSnapshot::layout_sources() const {
        const auto& recipe = definition();
        std::vector<graphic_resource::BindingLayout> result;
        result.reserve(recipe.binding_layouts.size());
        for (size_t i = 0; i < recipe.binding_layouts.size(); ++i)
            result.push_back(detail::DescriptorAccess::FromOwned(recipe.sources.at(recipe.binding_sources.at(i)).alias(recipe.binding_layouts[i])));
        return result;
    }
    PipelineRecipeDraft::PipelineRecipeDraft(LogicalDeviceAccessor device) {
        init_start(std::move(device));
    }
    PipelineRecipeSnapshot PipelineRecipeDraft::snapshot() const {
        return {get_device(), std::make_shared<impl::PipelineRecipe>(edit_recipe())};
    }
    GraphicsPipeline make_graphics_pipeline(PipelineRecipeSnapshot snapshot, RenderPassAccessor pass, std::uint32_t subpass, typed::HeapStorage) {
        validate_target(snapshot.device(), pass, subpass);
        const auto& recipe = snapshot.definition();
        if (recipe.pending_layouts)
            throw std::logic_error("Pipeline descriptor layouts have not been materialized");
        auto owner = make_heap_pipeline_layout(snapshot.device(), recipe.binding_layouts, recipe.push_ranges);
        const PipelineLayoutAccessor layout(owner);
        const operation::GraphicsPipelineInput input{owner.get(), pass.data_.get(), &recipe.graphics_state, recipe.shaders, subpass};
        return GraphicsPipeline(PipelineAccessor{make_heap_pipeline(snapshot.device(), layout.owner_ref(), input)});
    }
    GraphicsPipeline make_graphics_pipeline(PipelineRecipeSnapshot snapshot, RenderPassAccessor pass, std::uint32_t subpass, typed::ArenaStorage<>) {
        validate_target(snapshot.device(), pass, subpass);
        GraphicsPipelineState state;
        state.edit().get() = snapshot.definition().graphics_state;
        const std::array<ResourceInit, 2> inputs{
            layout_input(snapshot), GraphicsPipelineInit{LAYOUT, std::move(pass), std::move(state), shader_inputs(snapshot.definition()), subpass}};
        ResourceArena arena(snapshot.device(), PLAN, inputs);
        return GraphicsPipeline(arena.pipeline(PIPELINE));
    }
    ComputePipeline make_compute_pipeline(PipelineRecipeSnapshot snapshot, typed::HeapStorage) {
        const auto& recipe = snapshot.definition();
        if (recipe.pending_layouts)
            throw std::logic_error("Pipeline descriptor layouts have not been materialized");
        if (recipe.shaders.size() != 1)
            throw std::invalid_argument("Compute pipeline requires exactly one compute shader");
        auto owner = make_heap_pipeline_layout(snapshot.device(), recipe.binding_layouts, recipe.push_ranges);
        const PipelineLayoutAccessor layout(owner);
        const operation::ComputePipelineInput input{owner.get(), recipe.shaders.front()};
        return ComputePipeline(PipelineAccessor{make_heap_pipeline(snapshot.device(), layout.owner_ref(), input)});
    }
    ComputePipeline make_compute_pipeline(PipelineRecipeSnapshot snapshot, typed::ArenaStorage<>) {
        auto shaders = shader_inputs(snapshot.definition());
        if (shaders.size() != 1)
            throw std::invalid_argument("Compute pipeline requires exactly one compute shader");
        const std::array<ResourceInit, 2> inputs{layout_input(snapshot), ComputePipelineInit{LAYOUT, std::move(shaders.front())}};
        ResourceArena arena(snapshot.device(), PLAN, inputs);
        return ComputePipeline(arena.pipeline(PIPELINE));
    }
} // namespace VSL_NAMESPACE::lifetime
