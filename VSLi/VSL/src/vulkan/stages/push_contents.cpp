#include <VSL/lifetime/pipeline_recipe_access.hpp>
#include <VSL/vulkan/stages/push_contents.hpp>
#include <VSL/operation/pipeline_layout_ops.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::PipelineRecipeAccess;
}

void VSL_NAMESPACE::pipeline_layout::PushConstants::injection(PipelineLayoutEditAccessor pl) {
    auto ranges = operation::normalize_push_ranges(*pl.get_device().data_, definitions);
    PipelineRecipeAccess::Edit(pl).push_ranges.swap(ranges);
}
