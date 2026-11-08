#pragma once
#include <VSL/operation/synchronization_ops.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct SynchronizationOwnerContext {
        std::weak_ptr<impl::SynchroManagerImpl> registry;
    };
    struct SemaphoreSlotStorage {
        alignas(impl::SemaphoreImpl) std::byte value[sizeof(impl::SemaphoreImpl)];
        alignas(SynchronizationOwnerContext) std::byte context[sizeof(SynchronizationOwnerContext)];
    };
    struct FenceSlotStorage {
        alignas(impl::FenceImpl) std::byte value[sizeof(impl::FenceImpl)];
        alignas(SynchronizationOwnerContext) std::byte context[sizeof(SynchronizationOwnerContext)];
    };
    void construct_semaphore(void* storage, LogicalDeviceAccessor device, SemaphoreCreateInfo info);
    void destroy_semaphore(void* storage) noexcept;
    impl::SemaphoreImpl& semaphore_at(void* storage);
    void construct_fence(void* storage, LogicalDeviceAccessor device, FenceCreateInfo info);
    void destroy_fence(void* storage) noexcept;
    impl::FenceImpl& fence_at(void* storage);
    std::shared_ptr<impl::SemaphoreImpl> make_heap_semaphore(LogicalDeviceAccessor device, SemaphoreCreateInfo info);
    std::shared_ptr<impl::FenceImpl> make_heap_fence(LogicalDeviceAccessor device, FenceCreateInfo info);
} // namespace VSL_NAMESPACE::lifetime
