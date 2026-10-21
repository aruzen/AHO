#include <VSL/vulkan/stages/depth_stencil.hpp>
#include <VSL/vulkan/_pimpls.hpp>
void VSL_NAMESPACE::pipeline_layout::DepthStencil::injection(PipelineLayoutEditAccessor pl) {
    auto convert = [](const StencilFace& face) {
        return vk::StencilOpState{.failOp = static_cast<vk::StencilOp>(face.fail),
                                  .passOp = static_cast<vk::StencilOp>(face.pass),
                                  .depthFailOp = static_cast<vk::StencilOp>(face.depth_fail),
                                  .compareOp = static_cast<vk::CompareOp>(face.compare),
                                  .compareMask = face.compare_mask,
                                  .writeMask = face.write_mask,
                                  .reference = face.reference};
    };
    pl.data_->info->depth_stencil =
        vk::PipelineDepthStencilStateCreateInfo{.depthTestEnable = depth_test,
                                                .depthWriteEnable = depth_write,
                                                .depthCompareOp = static_cast<vk::CompareOp>(depth_compare),
                                                .stencilTestEnable = stencil_test,
                                                .front = convert(front),
                                                .back = convert(back),
                                                .minDepthBounds = 0.f,
                                                .maxDepthBounds = 1.f};
}
