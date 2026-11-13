#pragma once
#include <VSL/vulkan/device.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <type_traits>

namespace VSL_NAMESPACE::lifetime::detail {
    struct StartupAccess {
        static const OwnedRef<impl::VulkanImplAccessor>& Owned(const VulkanAccessor& value) { return value._owner; }
        static OwnedRef<impl::VulkanImplAccessor>& Owned(VulkanAccessor& value) { return value._owner; }
        static OwnedRef<impl::VulkanImplAccessor> Owned(VulkanAccessor&& value) { return std::move(value._owner); }
        static OwnedRef<impl::VulkanImplAccessor> Owned(const VulkanAccessor&& value) { return value._owner; }
        template<bool V>
        static const OwnedRef<impl::VulkanImplAccessor>& Owned(const Vulkan<V>& value) { return value._value._owner; }
        template<bool V>
        static OwnedRef<impl::VulkanImplAccessor>& Owned(Vulkan<V>& value) { return value._value._owner; }
        template<bool V>
        static OwnedRef<impl::VulkanImplAccessor> Owned(Vulkan<V>&& value) { return std::move(value._value._owner); }
        template<bool V>
        static OwnedRef<impl::VulkanImplAccessor> Owned(const Vulkan<V>&& value) { return value._value._owner; }
        static const OwnedRef<impl::PhysicalDeviceImpl>& Owned(const PhysicalDevice& value) { return value._owner; }
        static OwnedRef<impl::PhysicalDeviceImpl>& Owned(PhysicalDevice& value) { return value._owner; }
        static OwnedRef<impl::PhysicalDeviceImpl> Owned(PhysicalDevice&& value) { return std::move(value._owner); }
        static OwnedRef<impl::PhysicalDeviceImpl> Owned(const PhysicalDevice&& value) { return value._owner; }
        template<typename S>
        static const std::shared_ptr<impl::PhysicalDevicesImpl>& Collection(const PhysicalDevices<S>& value) { return value._collection._state; }
        template<typename S>
        static std::shared_ptr<impl::PhysicalDevicesImpl>& Collection(PhysicalDevices<S>& value) { return value._collection._state; }
        template<typename S>
        static std::shared_ptr<impl::PhysicalDevicesImpl> Collection(PhysicalDevices<S>&& value) { return std::move(value._collection._state); }
        template<typename S>
        static std::shared_ptr<impl::PhysicalDevicesImpl> Collection(const PhysicalDevices<S>&& value) { return value._collection._state; }
        static VulkanAccessor FromOwned(OwnedRef<impl::VulkanImplAccessor> owner);
        static PhysicalDevice FromOwned(OwnedRef<impl::PhysicalDeviceImpl> owner);
        static VulkanAccessor FromShared(std::shared_ptr<impl::VulkanImplAccessor> owner);
        static PhysicalDevice FromShared(std::shared_ptr<impl::PhysicalDeviceImpl> owner);
        static void RequireCanonical(const OwnedRef<impl::VulkanImplAccessor>& owner);
        static void RequireCanonical(const OwnedRef<impl::PhysicalDeviceImpl>& owner);
        template<typename T>
        static decltype(auto) Shared(T&& value);
    };
    // 既存Shared境界への接続。通常操作はOwnedから実体を直接参照する。
    template<typename T>
    decltype(auto) StartupAccess::Shared(T&& value) {
        if constexpr (std::is_lvalue_reference_v<T&&>)
            return OwnerAccess::Shared(Owned(value));
        else
            return OwnerAccess::TakeShared(Owned(std::forward<T>(value)));
    }
} // namespace VSL_NAMESPACE::lifetime::detail
