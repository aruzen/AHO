#include <VSL/vulkan/stages/id_picking.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
void vsl::pipeline_layout::IDPicking::injection(PipelineLayoutAccessor pl) {
    ColorBlend(ColorBlendAttachment{.enabled = false, .writeMask = ColorWriteMask::R}).injection(pl);
}
