#include <VSL/operation/graphics_pipeline_state_access.hpp>
#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::operation::detail::GraphicsPipelineStateAccess;
}

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    injection(pl.edit_state());
}

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::GraphicsPipelineStateEditRef state) {
    auto& info = GraphicsPipelineStateAccess::Edit(state);

    info.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    info.input_assembly.primitiveRestartEnable = false;
}
