#include <VSL/vulkan/stages/push_contents.hpp>
#include <VSL/operation/pipeline_layout_ops.hpp>

void VSL_NAMESPACE::pipeline_layout::PushConstants::injection(PipelineLayoutEditAccessor pl) {
    auto ranges = operation::normalize_push_ranges(*pl.get_device().data_, definitions);
    pl.edit_recipe().push_ranges.swap(ranges);
}
