#pragma once
#include <VSL/operation/frame_buffer_ops.hpp>
#include <VSL/vulkan/frame_buffer.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct FrameBufferOwnerContext {
        WeakOwnedRef<impl::RenderPassImpl> pass;
    };
    struct FrameBufferSlotStorage {
        alignas(impl::FrameBufferImpl) std::byte value[sizeof(impl::FrameBufferImpl)];
        alignas(FrameBufferOwnerContext) std::byte context[sizeof(FrameBufferOwnerContext)];
    };
    void construct_frame_buffer(void* storage, LogicalDeviceAccessor device, operation::FrameBufferInput input);
    impl::FrameBufferImpl& frame_buffer_at(void* storage);
    void destroy_frame_buffer(void* storage) noexcept;
    void set_frame_buffer_pass_owner(impl::FrameBufferImpl& target, OwnedRef<impl::RenderPassImpl> owner);
    std::shared_ptr<impl::FrameBufferImpl> make_heap_frame_buffer(LogicalDeviceAccessor device, operation::FrameBufferInput input,
                                                                  OwnerRef pass, std::vector<OwnerRef> dependencies);
} // namespace VSL_NAMESPACE::lifetime
