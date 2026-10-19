#include <VSL/vulkan/stages/id_picking.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
void VSL_NAMESPACE::pipeline_layout::IDPicking::injection(PipelineLayoutAccessor pl) {
    ColorBlend(ColorBlendAttachment{.enabled = false, .writeMask = ColorWriteMask::R}).injection(pl);
}
