#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void VSL_NAMESPACE::pipeline_layout::ColorBlend::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	auto& colorBlendAttachment = std::any_cast<vk::PipelineColorBlendAttachmentState&>
		(info.pool["colorBlendAttachment"] = vk::PipelineColorBlendAttachmentState());

	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR
		| vk::ColorComponentFlagBits::eG
		| vk::ColorComponentFlagBits::eB
		| vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = false;
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
	colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
	colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional

	info.colorBlend.logicOpEnable = false;
	info.colorBlend.logicOp = vk::LogicOp::eCopy; // Optional
	info.colorBlend.attachmentCount = 1;
	info.colorBlend.pAttachments = &colorBlendAttachment;
	info.colorBlend.blendConstants[0] = 0.0f; // Optional
	info.colorBlend.blendConstants[1] = 0.0f; // Optional
	info.colorBlend.blendConstants[2] = 0.0f; // Optional
	info.colorBlend.blendConstants[3] = 0.0f; // Optional
}
