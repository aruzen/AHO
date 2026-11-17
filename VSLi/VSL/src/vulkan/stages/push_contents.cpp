#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/pipeline_recipe_access.hpp>
#include <VSL/vulkan/stages/push_contents.hpp>
#include <VSL/operation/pipeline_layout_ops.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::PipelineRecipeAccess;
}

void VSL_NAMESPACE::pipeline_layout::PushConstants::injection(PipelineLayoutEditAccessor pl) {
    auto ranges = operation::normalize_push_ranges(*StartupAccess::Owned(pl.get_device()), definitions);
    PipelineRecipeAccess::Edit(pl).push_ranges.swap(ranges);
}
