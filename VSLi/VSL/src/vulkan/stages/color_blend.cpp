#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/_pimpls.hpp>
namespace VSL_NAMESPACE::pipeline_layout {
    void ColorBlend::injection(PipelineLayoutAccessor pl) {
        auto& info = *pl._data->info;
        if (!info.pool.contains("colorBlendAttachments"))
            info.pool["colorBlendAttachments"] = std::vector<vk::PipelineColorBlendAttachmentState>{};
        auto& states = std::any_cast<std::vector<vk::PipelineColorBlendAttachmentState>&>(
            info.pool["colorBlendAttachments"]);
        for (const auto& a : attachments)
            states.push_back(
                {.blendEnable = a.enabled,
                 .srcColorBlendFactor = static_cast<vk::BlendFactor>(a.sourceColor),
                 .dstColorBlendFactor = static_cast<vk::BlendFactor>(a.destinationColor),
                 .colorBlendOp = static_cast<vk::BlendOp>(a.colorOp),
                 .srcAlphaBlendFactor = static_cast<vk::BlendFactor>(a.sourceAlpha),
                 .dstAlphaBlendFactor = static_cast<vk::BlendFactor>(a.destinationAlpha),
                 .alphaBlendOp = static_cast<vk::BlendOp>(a.alphaOp),
                 .colorWriteMask = vk::ColorComponentFlags(static_cast<std::uint32_t>(a.writeMask))});
        info.colorBlend.logicOpEnable = false;
        info.colorBlend.attachmentCount = static_cast<std::uint32_t>(states.size());
        info.colorBlend.pAttachments = states.data();
    }
} // namespace VSL_NAMESPACE::pipeline_layout
