#pragma once
#include <VSL/vulkan/synchronize.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct SynchronizationAccess {
        static const OwnedRef<impl::FenceImpl>& Owned(const FenceHolder& value) { return value._owner; }
        static OwnedRef<impl::FenceImpl>& Owned(FenceHolder& value) { return value._owner; }
        static OwnedRef<impl::FenceImpl> Owned(FenceHolder&& value) { return std::move(value._owner); }
        static OwnedRef<impl::FenceImpl> Owned(const FenceHolder&& value) { return value._owner; }
        static const OwnedRef<impl::SemaphoreImpl>& Owned(const SemaphoreHolder& value) { return value._owner; }
        static OwnedRef<impl::SemaphoreImpl>& Owned(SemaphoreHolder& value) { return value._owner; }
        static OwnedRef<impl::SemaphoreImpl> Owned(SemaphoreHolder&& value) { return std::move(value._owner); }
        static OwnedRef<impl::SemaphoreImpl> Owned(const SemaphoreHolder&& value) { return value._owner; }
        static const std::shared_ptr<impl::SynchroManagerImpl>& Registry(const SynchroManager& value) { return value._registry; }
        static std::shared_ptr<impl::SynchroManagerImpl>& Registry(SynchroManager& value) { return value._registry; }
        static std::shared_ptr<impl::SynchroManagerImpl> Registry(SynchroManager&& value) { return std::move(value._registry); }
        static std::shared_ptr<impl::SynchroManagerImpl> Registry(const SynchroManager&& value) { return value._registry; }
        static FenceHolder FromOwned(OwnedRef<impl::FenceImpl> owner);
        static SemaphoreHolder FromOwned(OwnedRef<impl::SemaphoreImpl> owner);
        static FenceHolder FromShared(std::shared_ptr<impl::FenceImpl> owner);
        static SemaphoreHolder FromShared(std::shared_ptr<impl::SemaphoreImpl> owner);
        static void RequireCanonical(const std::shared_ptr<impl::FenceImpl>& owner);
        static void RequireCanonical(const std::shared_ptr<impl::SemaphoreImpl>& owner);
    };
} // namespace VSL_NAMESPACE::lifetime::detail
