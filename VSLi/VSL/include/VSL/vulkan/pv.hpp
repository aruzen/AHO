#pragma once
#include "../define.hpp"

namespace VSL_NAMESPACE::impl {
    struct VulkanImplAccessor;
    template<bool v>
    struct VulkanImpl;
    struct PhysicalDeviceImpl;
    struct PhysicalDevicesImpl;
    struct LogicalDeviceImpl;
    struct CommandPoolImpl;
    struct CommandBufferImpl;
    struct CommandImpl;
    struct CommandManagerImpl;
    struct SubmissionImpl;
    struct SurfaceImpl;
    struct SwapchainImpl;
    struct ViewImpl;
    struct CreateInfo;
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
