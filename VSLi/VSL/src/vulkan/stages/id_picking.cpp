//
// Created by morimoto_hibiki on 2026/01/03.
//

#include <VSL/vulkan/stages/id_picking.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::IDPicking::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
    auto& info = *pl._data->info;

    if (not info.pool.contains("colorBlendAttachments")) {
        info.pool["colorBlendAttachments"] = std::vector<vk::PipelineColorBlendAttachmentState>();
    }
    auto& colorBlendAttachments = std::any_cast<std::vector<vk::PipelineColorBlendAttachmentState>&>
            (info.pool["colorBlendAttachments"]);
    colorBlendAttachments.emplace_back();
    auto& id_picking = colorBlendAttachments.back();

    id_picking.colorWriteMask = vk::ColorComponentFlagBits::eR;
    id_picking.blendEnable = false;
    id_picking.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional
    id_picking.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
    id_picking.colorBlendOp = vk::BlendOp::eAdd; // Optional
    id_picking.srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
    id_picking.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
    id_picking.alphaBlendOp = vk::BlendOp::eAdd; // Optional

    info.colorBlend.logicOpEnable = false;
    info.colorBlend.logicOp = vk::LogicOp::eCopy; // Optional
    info.colorBlend.attachmentCount = colorBlendAttachments.size();
    info.colorBlend.pAttachments = colorBlendAttachments.data();
    info.colorBlend.blendConstants[0] = 0.0f; // Optional
    info.colorBlend.blendConstants[1] = 0.0f; // Optional
    info.colorBlend.blendConstants[2] = 0.0f; // Optional
    info.colorBlend.blendConstants[3] = 0.0f; // Optional
}

