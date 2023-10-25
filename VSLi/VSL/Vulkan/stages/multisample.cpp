#include "pch.h"
#include "Multisample.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::Multisample::injection(VSL_NAMESPACE::_impl::CreateInfo& info)
{
	info.multisample.sampleShadingEnable = false;
	info.multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
	info.multisample.minSampleShading = 1.0f; // Optional
	info.multisample.pSampleMask = nullptr; // Optional
	info.multisample.alphaToCoverageEnable = false; // Optional
	info.multisample.alphaToOneEnable = false; // Optional
}
