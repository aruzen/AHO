#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/stages/resource_binding.hpp>

#include <utility>

VSL_NAMESPACE::pipeline_layout::ResourceBinding::ResourceBinding(std::vector<graphic_resource::BindingLayout> sets) : resourceBindingSets(std::move(sets)) {}

void VSL_NAMESPACE::pipeline_layout::ResourceBinding::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    std::vector<vk::DescriptorSetLayout>& layouts = std::any_cast<std::vector<vk::DescriptorSetLayout>&>(pl._data->info->pool["DescriptorSetLayouts"] = std::vector<vk::DescriptorSetLayout>());
    pl._data->info->bindingLayouts.clear();
    layouts.resize(resourceBindingSets.size());
    for (size_t i = 0; i < resourceBindingSets.size(); i++) {
        layouts[i] = resourceBindingSets[i]._data->layout;
        pl._data->info->bindingLayouts.push_back(resourceBindingSets[i]._data);
    }

    pl._data->info->pipelineLayout.pSetLayouts = layouts.data();
    pl._data->info->pipelineLayout.setLayoutCount = layouts.size();
}
