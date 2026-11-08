#pragma once
#include <VSL/operation/surface_ops.hpp>
#include <VSL/lifetime/window_runtime.hpp>
#include <VSL/vulkan/surface.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct SurfaceOwnerContext {
        WeakOwnedRef<impl::VulkanImplAccessor> instance;
        WeakOwnedRef<impl::OSWindowImpl> window;
    };
    struct SurfaceSlotStorage {
        alignas(impl::SurfaceImpl) std::byte value[sizeof(impl::SurfaceImpl)];
        alignas(SurfaceOwnerContext) std::byte context[sizeof(SurfaceOwnerContext)];
    };
    void construct_surface(void* storage, impl::VulkanImplAccessor& instance, impl::OSWindowImpl& window, const operation::SurfaceNativeCalls& calls = {});
    void destroy_surface(void* storage) noexcept;
    impl::SurfaceImpl& surface_at(void* storage);
    void set_surface_owners(impl::SurfaceImpl& target, OwnedRef<impl::VulkanImplAccessor> instance, const OSWindowOwner& window);
    std::shared_ptr<impl::SurfaceImpl> make_heap_surface(VulkanAccessor instance, OSWindowOwner window);
} // namespace VSL_NAMESPACE::lifetime
