#pragma once
#include <VSL/operation/render_pass_ops.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct RenderPassSlotStorage {
        alignas(impl::RenderPassImpl) std::byte value[sizeof(impl::RenderPassImpl)];
    };
    void construct_render_pass(void* storage, LogicalDeviceAccessor device, RenderPassDescription description);
    void destroy_render_pass(void* storage) noexcept;
    impl::RenderPassImpl& render_pass_at(void* storage);
    std::shared_ptr<impl::RenderPassImpl> make_heap_render_pass(LogicalDeviceAccessor device, RenderPassDescription description);
} // namespace VSL_NAMESPACE::lifetime
