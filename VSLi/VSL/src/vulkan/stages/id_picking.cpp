#include <VSL/vulkan/stages/id_picking.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
void VSL_NAMESPACE::pipeline_layout::IDPicking::injection(PipelineLayoutEditAccessor pl) {
    ColorBlend(ColorBlendAttachment{.enabled = false, .write_mask = ColorWriteMask::R}).injection(pl);
}
