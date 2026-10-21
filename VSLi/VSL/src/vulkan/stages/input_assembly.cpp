#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = *pl.data_->info;

    info.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    info.input_assembly.primitiveRestartEnable = false;
}
