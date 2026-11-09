#include <VSL/operation/graphics_pipeline_state_access.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::operation::detail::GraphicsPipelineStateAccess;
}

namespace VSL_NAMESPACE::pipeline_layout {
    void ColorBlend::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
        injection(pl.edit_state());
    }

    void ColorBlend::injection(VSL_NAMESPACE::GraphicsPipelineStateEditRef state) {
        auto& info = GraphicsPipelineStateAccess::Edit(state);
        auto& states = info.blend_attachments;
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
    }
} // namespace VSL_NAMESPACE::pipeline_layout
