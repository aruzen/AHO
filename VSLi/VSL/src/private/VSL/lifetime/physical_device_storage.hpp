#pragma once
#include <VSL/operation/physical_device_ops.hpp>
#include <VSL/lifetime/owner_ref.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct PhysicalDeviceOwnerContext {
        WeakOwnedRef<impl::VulkanImplAccessor> instance;
    };
    struct PhysicalDeviceSlotStorage {
        alignas(impl::PhysicalDeviceImpl) std::byte value[sizeof(impl::PhysicalDeviceImpl)];
        alignas(PhysicalDeviceOwnerContext) std::byte context[sizeof(PhysicalDeviceOwnerContext)];
    };
    void construct_physical_device(void* storage, impl::VulkanImplAccessor& instance, const operation::PhysicalDeviceCandidate& candidate);
    void destroy_physical_device(void* storage) noexcept;
    impl::PhysicalDeviceImpl& physical_device_at(void* storage);
    void set_physical_device_instance_owner(impl::PhysicalDeviceImpl& target, OwnedRef<impl::VulkanImplAccessor> instance);
    std::shared_ptr<impl::PhysicalDeviceImpl> make_heap_physical_device(VulkanAccessor instance, const operation::PhysicalDeviceCandidate& candidate);
} // namespace VSL_NAMESPACE::lifetime
