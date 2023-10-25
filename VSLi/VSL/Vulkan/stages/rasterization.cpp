#include "pch.h"
#include "rasterization.h"
#include "../_pimpls.h"

void VSL_NAMESPACE::pipeline_layout::Rasterization::injection(VSL_NAMESPACE::_impl::CreateInfo& info)
{
	info.rasterization.depthClampEnable = false;
	info.rasterization.rasterizerDiscardEnable = false;
	info.rasterization.polygonMode = vk::PolygonMode::eFill;
	info.rasterization.lineWidth = 1.0f;
	info.rasterization.cullMode = vk::CullModeFlagBits::eBack;
	info.rasterization.frontFace = vk::FrontFace::eClockwise;
	info.rasterization.depthBiasEnable = false;
	info.rasterization.depthBiasConstantFactor = 0.0f; // Optional
	info.rasterization.depthBiasClamp = 0.0f; // Optional
	info.rasterization.depthBiasSlopeFactor = 0.0f; // Optional
}
