#pragma once
#include <VSL/vulkan/pipeline.hpp>
#include <VSL/vulkan/frame_buffer.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct GraphicsAccess {
        static const OwnedRef<impl::PipelineLayoutImpl>& Owned(const PipelineLayoutAccessor& value) { return value._owner; }
        static OwnedRef<impl::PipelineLayoutImpl>& Owned(PipelineLayoutAccessor& value) { return value._owner; }
        static OwnedRef<impl::PipelineLayoutImpl> Owned(PipelineLayoutAccessor&& value) { return std::move(value._owner); }
        static OwnedRef<impl::PipelineLayoutImpl> Owned(const PipelineLayoutAccessor&& value) { return value._owner; }
        static const OwnedRef<impl::PipelineLayoutImpl>& Owned(const PipelineLayoutEditAccessor& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineLayoutImpl>& Owned(PipelineLayoutEditAccessor& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineLayoutImpl> Owned(PipelineLayoutEditAccessor&& value) { return std::move(value._value._owner); }
        static OwnedRef<impl::PipelineLayoutImpl> Owned(const PipelineLayoutEditAccessor&& value) { return value._value._owner; }
        static const OwnedRef<impl::PipelineLayoutImpl>& Owned(const FrozenPipelineLayout& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineLayoutImpl>& Owned(FrozenPipelineLayout& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineLayoutImpl> Owned(FrozenPipelineLayout&& value) { return std::move(value._value._owner); }
        static OwnedRef<impl::PipelineLayoutImpl> Owned(const FrozenPipelineLayout&& value) { return value._value._owner; }
        static PipelineLayoutAccessor FromOwned(OwnedRef<impl::PipelineLayoutImpl> owner) { return PipelineLayoutAccessor(std::move(owner)); }
        static PipelineLayoutAccessor FromShared(std::shared_ptr<impl::PipelineLayoutImpl> owner);

        static const OwnedRef<impl::RenderPassImpl>& Owned(const RenderPassAccessor& value) { return value._owner; }
        static OwnedRef<impl::RenderPassImpl>& Owned(RenderPassAccessor& value) { return value._owner; }
        static OwnedRef<impl::RenderPassImpl> Owned(RenderPassAccessor&& value) { return std::move(value._owner); }
        static OwnedRef<impl::RenderPassImpl> Owned(const RenderPassAccessor&& value) { return value._owner; }
        static const OwnedRef<impl::RenderPassImpl>& Owned(const IDPickingRenderPass& value) { return value._pass._owner; }
        static OwnedRef<impl::RenderPassImpl>& Owned(IDPickingRenderPass& value) { return value._pass._owner; }
        static OwnedRef<impl::RenderPassImpl> Owned(IDPickingRenderPass&& value) { return value._pass._owner; }
        static OwnedRef<impl::RenderPassImpl> Owned(const IDPickingRenderPass&& value) { return value._pass._owner; }
        static RenderPassAccessor FromOwned(OwnedRef<impl::RenderPassImpl> owner) { return RenderPassAccessor(std::move(owner)); }
        static RenderPassAccessor FromShared(std::shared_ptr<impl::RenderPassImpl> owner);

        static const OwnedRef<impl::PipelineImpl>& Owned(const PipelineAccessor& value) { return value._owner; }
        static OwnedRef<impl::PipelineImpl>& Owned(PipelineAccessor& value) { return value._owner; }
        static OwnedRef<impl::PipelineImpl> Owned(PipelineAccessor&& value) { return std::move(value._owner); }
        static OwnedRef<impl::PipelineImpl> Owned(const PipelineAccessor&& value) { return value._owner; }
        static const OwnedRef<impl::PipelineImpl>& Owned(const GraphicsPipeline& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineImpl>& Owned(GraphicsPipeline& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineImpl> Owned(GraphicsPipeline&& value) { return std::move(value._value._owner); }
        static OwnedRef<impl::PipelineImpl> Owned(const GraphicsPipeline&& value) { return value._value._owner; }
        static const OwnedRef<impl::PipelineImpl>& Owned(const ComputePipeline& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineImpl>& Owned(ComputePipeline& value) { return value._value._owner; }
        static OwnedRef<impl::PipelineImpl> Owned(ComputePipeline&& value) { return std::move(value._value._owner); }
        static OwnedRef<impl::PipelineImpl> Owned(const ComputePipeline&& value) { return value._value._owner; }
        static PipelineAccessor FromOwned(OwnedRef<impl::PipelineImpl> owner) { return PipelineAccessor(std::move(owner)); }
        static PipelineAccessor FromShared(std::shared_ptr<impl::PipelineImpl> owner);

        static const OwnedRef<impl::FrameBufferImpl>& Owned(const FrameBufferAccessor& value) { return value._owner; }
        static OwnedRef<impl::FrameBufferImpl>& Owned(FrameBufferAccessor& value) { return value._owner; }
        static OwnedRef<impl::FrameBufferImpl> Owned(FrameBufferAccessor&& value) { return std::move(value._owner); }
        static OwnedRef<impl::FrameBufferImpl> Owned(const FrameBufferAccessor&& value) { return value._owner; }
        static FrameBufferAccessor FromOwned(OwnedRef<impl::FrameBufferImpl> owner) { return FrameBufferAccessor(std::move(owner)); }
        static FrameBufferAccessor FromShared(std::shared_ptr<impl::FrameBufferImpl> owner);
    };
} // namespace VSL_NAMESPACE::lifetime::detail
