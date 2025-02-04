#pragma once
#include "../define.h"

namespace VSL_NAMESPACE::_impl {
	struct Vulkan_impl_accessor;
	template<bool V>
	struct Vulkan_impl;
	struct PhysicalDevice_impl;
	struct PhysicalDevices_impl;
	struct LogicalDevice_impl;
	struct CommandPool_impl;
	struct CommandBuffer_impl;
	struct Command_impl;
	struct CommandManager_impl;
	struct Surface_impl;
	struct Swapchain_impl;
	struct View_impl;
	// struct Viewport_impl;
	struct CreateInfo;
	struct PipelineLayout_impl;
	// struct ShaderStage_impl;
	struct Shader_impl; 
	struct RenderPass_impl;
	struct Pipeline_impl;
	struct FrameBuffer_impl;
	struct Semaphore_impl;
	struct Fence_impl;
	struct SynchroManager_impl;

	namespace pipeline_layout {
		struct ShaderGroup_impl;
	}
}