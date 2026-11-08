#pragma once
#include <VSL/operation/pipeline_layout_create_info.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::operation {
    void validate_pipeline_device(const impl::LogicalDeviceImpl& device);
    std::vector<PushConstantRange> normalize_push_ranges(const impl::LogicalDeviceImpl& device, std::span<const PushConstantRange> definitions);
    void initialize_pipeline_layout(impl::PipelineLayoutImpl& target, impl::LogicalDeviceImpl& device,
                                    std::span<impl::ResourceBindingLayoutImpl* const> layouts, std::span<const PushConstantRange> ranges);
    bool pipeline_layout_matches(const impl::PipelineLayoutImpl& target, std::span<impl::ResourceBindingLayoutImpl* const> layouts,
                                 std::span<const PushConstantRange> ranges);
    void destroy_pipeline_layout(impl::PipelineLayoutImpl& target) noexcept;
    // 借用するrecipeはnative呼出しが終わるまで生存し、不変でなければならない。
    struct PipelineScratch {
        vk::PipelineDynamicStateCreateInfo dynamic_state;
        vk::PipelineVertexInputStateCreateInfo vertex_input;
        vk::PipelineViewportStateCreateInfo viewport;
        vk::PipelineColorBlendStateCreateInfo color_blend;
        explicit PipelineScratch(const impl::PipelineRecipe& recipe);
    };
} // namespace VSL_NAMESPACE::operation
