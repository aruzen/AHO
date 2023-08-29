#pragma once
#include "../define.h"

namespace VSL_NAMESPACE::_impl {
	template<bool V>
	struct Vulkan_impl;
	struct PhysicalDevice_impl;
	struct PhysicalDevices_impl;
	struct LogicalDevice_impl;
	struct CommandManager_impl;
	struct Surface_impl;
	struct Swapchain_impl;
	struct View_impl;
	// struct Viewport_impl;
	struct CreateInfo;
	struct PipelineLayout_impl;
	struct ShaderStage_impl;
	struct Shader_impl;
	struct RenderPass_impl;
}