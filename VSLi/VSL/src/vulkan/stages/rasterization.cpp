#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::Rasterization::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	info.rasterization.depthClampEnable = false;
	info.rasterization.rasterizerDiscardEnable = false;
	info.rasterization.polygonMode = vk::PolygonMode::eFill;
	info.rasterization.lineWidth = 1.0f;
	info.rasterization.cullMode = vk::CullModeFlagBits::eBack;
	info.rasterization.frontFace = vk::FrontFace::eCounterClockwise;
    // info.rasterization.frontFace = vk::FrontFace::eCounterClockwise;
	info.rasterization.depthBiasEnable = false;
	info.rasterization.depthBiasConstantFactor = 0.0f; // Optional
	info.rasterization.depthBiasClamp = 0.0f; // Optional
	info.rasterization.depthBiasSlopeFactor = 0.0f; // Optional
}
