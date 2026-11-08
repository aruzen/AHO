#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    injection(pl.edit_state());
}

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::GraphicsPipelineStateEditRef state) {
    auto& info = state.get();

    info.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    info.input_assembly.primitiveRestartEnable = false;
}
