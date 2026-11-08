#pragma once
#include <VSL/operation/device_ops.hpp>
#include <VSL/lifetime/owner_ref.hpp>
#include <VSL/lifetime/dependency_node.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct DeviceOwnerContext {
        WeakOwnedRef<impl::PhysicalDeviceImpl> physical;
    };
    struct DeviceSlotStorage {
        alignas(impl::LogicalDeviceImpl) std::byte value[sizeof(impl::LogicalDeviceImpl)];
        alignas(DeviceOwnerContext) std::byte context[sizeof(DeviceOwnerContext)];
    };
    void construct_device(void* storage, impl::PhysicalDeviceImpl& physical, DeviceCreateInfo info, const operation::DeviceNativeCalls& calls = {});
    void destroy_device(void* storage) noexcept;
    impl::LogicalDeviceImpl& device_at(void* storage);
    void set_device_physical_owner(impl::LogicalDeviceImpl& target, OwnedRef<impl::PhysicalDeviceImpl> physical);
    std::shared_ptr<impl::LogicalDeviceImpl> make_heap_device(PhysicalDevice physical, DeviceCreateInfo info);
} // namespace VSL_NAMESPACE::lifetime
