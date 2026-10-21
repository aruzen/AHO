#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/_pimpls.hpp>
namespace VSL_NAMESPACE::pipeline_layout {
    void ColorBlend::injection(PipelineLayoutEditAccessor pl) {
        auto& info = *pl.data_->info;
        if (!info.pool.contains("colorBlendAttachments"))
            info.pool["colorBlendAttachments"] = std::vector<vk::PipelineColorBlendAttachmentState>{};
        auto& states = std::any_cast<std::vector<vk::PipelineColorBlendAttachmentState>&>(
            info.pool["colorBlendAttachments"]);
        for (const auto& a : attachments)
            states.push_back(
                {.blendEnable = a.enabled,
                 .srcColorBlendFactor = static_cast<vk::BlendFactor>(a.source_color),
                 .dstColorBlendFactor = static_cast<vk::BlendFactor>(a.destination_color),
                 .colorBlendOp = static_cast<vk::BlendOp>(a.color_op),
                 .srcAlphaBlendFactor = static_cast<vk::BlendFactor>(a.source_alpha),
                 .dstAlphaBlendFactor = static_cast<vk::BlendFactor>(a.destination_alpha),
                 .alphaBlendOp = static_cast<vk::BlendOp>(a.alpha_op),
                 .colorWriteMask = vk::ColorComponentFlags(static_cast<std::uint32_t>(a.write_mask))});
        info.color_blend.logicOpEnable = false;
        info.color_blend.attachmentCount = static_cast<std::uint32_t>(states.size());
        info.color_blend.pAttachments = states.data();
    }
} // namespace VSL_NAMESPACE::pipeline_layout
