#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/_pimpls.hpp>

VSL_NAMESPACE::pipeline_layout::Rasterization::Rasterization(
    CullingMode culling_mode,
    FrontFace front_face) : culling_mode(culling_mode), front_face(front_face) {}

void VSL_NAMESPACE::pipeline_layout::Rasterization::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl) {
    auto& info = *pl._data->info;

    info.rasterization.depthClampEnable = false;
    info.rasterization.rasterizerDiscardEnable = false;
    info.rasterization.polygonMode = vk::PolygonMode::eFill;
    info.rasterization.lineWidth = 1.0f;
    info.rasterization.cullMode = (vk::CullModeFlagBits)culling_mode;
    info.rasterization.frontFace = (vk::FrontFace)front_face;
    info.rasterization.depthBiasEnable = false;
    info.rasterization.depthBiasConstantFactor = 0.0f; // Optional
    info.rasterization.depthBiasClamp = 0.0f;          // Optional
    info.rasterization.depthBiasSlopeFactor = 0.0f;    // Optional
}
