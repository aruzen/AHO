#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/stages/resource_binding.hpp>

#include <utility>

VSL_NAMESPACE::pipeline_layout::ResourceBinding::ResourceBinding(std::vector<graphic_resource::BindingLayout> sets) : resource_binding_sets(std::move(sets)) {}

void VSL_NAMESPACE::pipeline_layout::ResourceBinding::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    std::vector<vk::DescriptorSetLayout>& layouts = std::any_cast<std::vector<vk::DescriptorSetLayout>&>(pl.data_->info->pool["DescriptorSetLayouts"] = std::vector<vk::DescriptorSetLayout>());
    pl.data_->info->binding_layouts.clear();
    layouts.resize(resource_binding_sets.size());
    for (size_t i = 0; i < resource_binding_sets.size(); i++) {
        layouts[i] = resource_binding_sets[i].data_->layout;
        pl.data_->info->binding_layouts.push_back(resource_binding_sets[i].data_);
    }

    pl.data_->info->pipeline_layout.pSetLayouts = layouts.data();
    pl.data_->info->pipeline_layout.setLayoutCount = layouts.size();
}
