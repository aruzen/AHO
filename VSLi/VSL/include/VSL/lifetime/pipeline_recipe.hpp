#pragma once
#include "../vulkan/pipeline_layout.hpp"
#include "../vulkan/pipeline.hpp"
#include "../operation/descriptor_create_info.hpp"
#include "../storage/typed_storage_policy.hpp"
#include "resource_arena.hpp"

namespace VSL_NAMESPACE::lifetime {
    class PipelineRecipeSnapshot {
        LogicalDeviceAccessor _device;
        std::shared_ptr<const impl::PipelineRecipe> _recipe;
        friend class PipelineRecipeDraft;
        PipelineRecipeSnapshot(LogicalDeviceAccessor device, std::shared_ptr<const impl::PipelineRecipe> recipe);

    public:
        LogicalDeviceAccessor device() const { return _device; }
        const impl::PipelineRecipe& definition() const&;
        const impl::PipelineRecipe& definition() const&& = delete;
        const std::optional<std::vector<DescriptorLayoutCreateInfo>>& pending_layouts() const&;
        const std::optional<std::vector<DescriptorLayoutCreateInfo>>& pending_layouts() const&& = delete;
        PipelineRecipeSnapshot with_layouts(std::span<const graphic_resource::BindingLayout> layouts) const;
        std::vector<graphic_resource::BindingLayout> layout_sources() const;
    };

    // 編集accessorを保存するstageがあっても、snapshotの内容は後から変わらない。
    class PipelineRecipeDraft : private PipelineLayoutEditAccessor {
    public:
        explicit PipelineRecipeDraft(LogicalDeviceAccessor device);
        PipelineRecipeDraft(const PipelineRecipeDraft&) = delete;
        PipelineRecipeDraft& operator=(const PipelineRecipeDraft&) = delete;
        template<pipeline_layout_injecter... Stages>
        void inject(Stages&&... stages) {
            helper::expansion_pipeline_layout_args(*this, std::forward<Stages>(stages)...);
        }
        PipelineRecipeSnapshot snapshot() const;
    };
    GraphicsPipeline make_graphics_pipeline(PipelineRecipeSnapshot recipe, RenderPassAccessor pass, std::uint32_t subpass, typed::HeapStorage);
    GraphicsPipeline make_graphics_pipeline(PipelineRecipeSnapshot recipe, RenderPassAccessor pass, std::uint32_t subpass, typed::ArenaStorage<>);
    ComputePipeline make_compute_pipeline(PipelineRecipeSnapshot recipe, typed::HeapStorage);
    ComputePipeline make_compute_pipeline(PipelineRecipeSnapshot recipe, typed::ArenaStorage<>);
    template<size_t capacity>
    PipelineRecipeSnapshot materialize_recipe(PipelineRecipeSnapshot recipe, typed::HeapStorage);
    template<size_t capacity>
    PipelineRecipeSnapshot materialize_recipe(PipelineRecipeSnapshot recipe, typed::ArenaStorage<>);

    template<size_t capacity>
    PipelineRecipeSnapshot materialize_recipe(PipelineRecipeSnapshot recipe, typed::HeapStorage) {
        if (!recipe.pending_layouts())
            return recipe;
        std::vector<graphic_resource::BindingLayout> layouts;
        layouts.reserve(recipe.pending_layouts()->size());
        std::optional<graphic_resource::BindingLayout> empty;
        size_t count = 0;
        for (const auto& info : *recipe.pending_layouts()) {
            if (info.bindings.empty() && empty) {
                layouts.push_back(*empty);
                continue;
            }
            if (count++ == capacity)
                throw std::length_error("Pipeline descriptor layout capacity exceeded");
            layouts.emplace_back(recipe.device(), info.bindings);
            if (info.bindings.empty())
                empty = layouts.back();
        }
        return recipe.with_layouts(layouts);
    }
    template<size_t capacity>
    PipelineRecipeSnapshot materialize_recipe(PipelineRecipeSnapshot recipe, typed::ArenaStorage<>) {
        if (!recipe.pending_layouts() || recipe.pending_layouts()->empty())
            return recipe.pending_layouts() ? recipe.with_layouts({}) : recipe;
        std::vector<typed::ResourceRequest> requests;
        std::vector<ResourceInit> inputs;
        std::vector<typed::ResourceId> sets;
        std::optional<typed::ResourceId> empty;
        for (const auto& info : *recipe.pending_layouts()) {
            if (info.bindings.empty() && empty) {
                sets.push_back(*empty);
                continue;
            }
            if (requests.size() == capacity)
                throw std::length_error("Pipeline descriptor layout capacity exceeded");
            const typed::ResourceId id{requests.size()};
            requests.push_back({id, storage::ResourceKind::DESCRIPTOR_LAYOUT});
            inputs.emplace_back(info);
            sets.push_back(id);
            if (info.bindings.empty())
                empty = id;
        }
        const typed::ResourcePlan<capacity> plan(requests, {typed::LifetimeScope::TRANSIENT, 0});
        ResourceArena arena(recipe.device(), plan, inputs);
        std::vector<graphic_resource::BindingLayout> layouts;
        layouts.reserve(sets.size());
        for (const auto id : sets)
            layouts.push_back(arena.descriptor_layout(id));
        return recipe.with_layouts(layouts);
    }
} // namespace VSL_NAMESPACE::lifetime
