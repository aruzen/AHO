#include <VSL/vulkan/stages/depth_stencil.hpp>
#include <VSL/vulkan/_pimpls.hpp>
void vsl::pipeline_layout::DepthStencil::injection(PipelineLayoutAccessor pl) {
    auto convert = [](const StencilFace &face) {
        return vk::StencilOpState{.failOp = static_cast<vk::StencilOp>(face.fail),
                                  .passOp = static_cast<vk::StencilOp>(face.pass),
                                  .depthFailOp = static_cast<vk::StencilOp>(face.depthFail),
                                  .compareOp = static_cast<vk::CompareOp>(face.compare),
                                  .compareMask = face.compareMask,
                                  .writeMask = face.writeMask,
                                  .reference = face.reference};
    };
    pl._data->info->depthStencil =
        vk::PipelineDepthStencilStateCreateInfo{.depthTestEnable = depthTest,
                                                .depthWriteEnable = depthWrite,
                                                .depthCompareOp = static_cast<vk::CompareOp>(depthCompare),
                                                .stencilTestEnable = stencilTest,
                                                .front = convert(front),
                                                .back = convert(back),
                                                .minDepthBounds = 0.f,
                                                .maxDepthBounds = 1.f};
}
