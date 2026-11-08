#pragma once
#include "../define.hpp"

namespace VSL_NAMESPACE::impl {
    struct VulkanImplAccessor;
    template<bool v>
    using VulkanImpl = VulkanImplAccessor;
    struct PhysicalDeviceImpl;
    struct PhysicalDevicesImpl;
    struct LogicalDeviceImpl;
    struct CommandPoolImpl;
    struct CommandBufferImpl;
    struct CommandImpl;
    struct CommandManagerImpl;
    struct SubmissionImpl;
    struct SubmissionWork;
    struct SurfaceImpl;
    struct OSWindowImpl;
    struct SwapchainImpl;
    struct ViewImpl;
    struct PipelineRecipe;
    struct PipelineLayoutImpl;
    struct ShaderImpl;
    struct RenderPassImpl;
    struct PipelineImpl;
    struct FrameBufferImpl;
    struct SemaphoreImpl;
    struct FenceImpl;
    struct SynchroManagerImpl;
    struct BufferImpl;
    struct ResourceBindingLayoutImpl;
    struct GraphicResourcePoolImpl;
    struct GraphicResourceManagerImpl;
    struct GraphicResourceManagerIdentity;
    struct GraphicResourceImpl;
    struct ImageImpl;
    struct SamplerImpl;
    namespace pipeline_layout {
        struct ShaderGroupImpl;
    }
} // namespace VSL_NAMESPACE::impl
