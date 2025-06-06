#ifdef _MSC_VER
#include "pch.h"
#endif
#include "multisample.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::Multisample::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	info.multisample.sampleShadingEnable = false;
	info.multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
	info.multisample.minSampleShading = 1.0f; // Optional
	info.multisample.pSampleMask = nullptr; // Optional
	info.multisample.alphaToCoverageEnable = false; // Optional
	info.multisample.alphaToOneEnable = false; // Optional
}
