#ifdef _MSC_VER
#include "pch.h"
#endif
#include "color_blend.h"
#include "../_pimpls.h"
#include "resource_binding.h"

vsl::pipeline_layout::ResourceBinding::ResourceBinding(std::vector<ResourceBindingLayout> sets) : resourceBindingSets(sets) {}

void VSL_NAMESPACE::pipeline_layout::ResourceBinding::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
    std::vector<vk::DescriptorSetLayout>& layouts
    = std::any_cast<std::vector<vk::DescriptorSetLayout>&>(pl._data->info->pool["DescriptorSetLayouts"] = std::vector<vk::DescriptorSetLayout>());
    layouts.resize(resourceBindingSets.size());
    for (int i = 0; i < resourceBindingSets.size(); i++)
        layouts[i] = resourceBindingSets[i]._data->layout;

    pl._data->info->pipelineLayout.pSetLayouts = layouts.data();
    pl._data->info->pipelineLayout.setLayoutCount = layouts.size();
}