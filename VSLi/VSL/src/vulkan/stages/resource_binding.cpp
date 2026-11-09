#include <VSL/vulkan/stages/resource_binding.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <utility>

VSL_NAMESPACE::pipeline_layout::ResourceBinding::ResourceBinding(std::vector<graphic_resource::BindingLayout> sets)
    : resource_binding_sets(std::move(sets)) {}

void VSL_NAMESPACE::pipeline_layout::ResourceBinding::injection(PipelineLayoutEditAccessor pl) {
    lifetime::replace_recipe_layouts(pl.edit_recipe(), pl.get_device(), resource_binding_sets);
}

void VSL_NAMESPACE::lifetime::replace_recipe_layouts(impl::PipelineRecipe& info, LogicalDeviceAccessor device,
                                                     std::span<const graphic_resource::BindingLayout> definitions,
                                                     std::optional<std::vector<DescriptorLayoutCreateInfo>> pending) {
    std::vector<impl::ResourceBindingLayoutImpl*> layouts;
    std::vector<size_t> binding_sources;
    std::vector<lifetime::OwnerRef> sources;
    std::vector<size_t> shader_sources;
    shader_sources.reserve(info.shader_sources.size());
    for (const auto index : info.shader_sources)
        shader_sources.push_back(lifetime::retain_recipe_owner(sources, info.sources.at(index)));
    layouts.reserve(definitions.size());
    binding_sources.reserve(definitions.size());
    for (const auto& layout : definitions) {
        if (!layout.data_ || !layout.data_.use_count() || !layout.data_->layout || layout.data_->device != device.data_.get())
            throw std::invalid_argument("Pipeline binding requires owning layouts on the same device");
        binding_sources.push_back(lifetime::retain_recipe_owner(sources, lifetime::detail::owned_ref(layout.data_, layout.data_->owner_node)));
        layouts.push_back(layout.data_.get());
    }
    info.binding_layouts.swap(layouts);
    info.binding_sources.swap(binding_sources);
    info.shader_sources.swap(shader_sources);
    info.sources.swap(sources);
    info.pending_layouts.swap(pending);
}
