#include <VSL/vulkan/stages/multisample.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::Multisample::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    injection(pl.edit_state());
}

void VSL_NAMESPACE::pipeline_layout::Multisample::injection(VSL_NAMESPACE::GraphicsPipelineStateEditRef state) {
    auto& info = state.get();

    info.multisample.sampleShadingEnable = false;
    info.multisample.rasterizationSamples = static_cast<vk::SampleCountFlagBits>(samples);
    info.multisample.minSampleShading = 1.0f;
    info.multisample.pSampleMask = nullptr;
    info.multisample.alphaToCoverageEnable = false;
    info.multisample.alphaToOneEnable = false;
}
