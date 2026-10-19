#include <VSL/vulkan/stages/multisample.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::Multisample::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	info.multisample.sampleShadingEnable = false;
	info.multisample.rasterizationSamples = static_cast<vk::SampleCountFlagBits>(samples);
	info.multisample.minSampleShading = 1.0f;
	info.multisample.pSampleMask = nullptr;
	info.multisample.alphaToCoverageEnable = false;
	info.multisample.alphaToOneEnable = false;
}
