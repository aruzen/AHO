#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::InputAssembly::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = *pl._data->info;

    info.inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    info.inputAssembly.primitiveRestartEnable = false;
}
