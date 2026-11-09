#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include <memory>
#include <map>
#include <optional>

#include "device.hpp"
#include "../operation/synchronization_create_info.hpp"
#include "../lifetime/owner_ref.hpp"

namespace VSL_NAMESPACE::lifetime::detail {
    struct SynchronizationAccess;
}

namespace VSL_NAMESPACE {
    struct SemaphoreHolder {
    private:
        friend struct lifetime::detail::SynchronizationAccess;
        lifetime::OwnedRef<impl::SemaphoreImpl> _owner;
        explicit SemaphoreHolder(lifetime::OwnedRef<impl::SemaphoreImpl> owner) : _owner(std::move(owner)) {}

    public:
        SemaphoreHolder() = default;
        SemaphoreHolder(const SemaphoreHolder&) = default;
        SemaphoreHolder(SemaphoreHolder&&) noexcept = default;
        SemaphoreHolder& operator=(SemaphoreHolder replacement) noexcept {
            swap(replacement);
            return *this;
        }
        void swap(SemaphoreHolder& other) noexcept { _owner.swap(other._owner); }
        explicit operator bool() const noexcept { return bool(_owner); }

        std::size_t size() const;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
        void destroy();
    };

    struct FenceHolder {
    private:
        friend struct lifetime::detail::SynchronizationAccess;
        lifetime::OwnedRef<impl::FenceImpl> _owner;
        explicit FenceHolder(lifetime::OwnedRef<impl::FenceImpl> owner) : _owner(std::move(owner)) {}

    public:
        FenceHolder() = default;
        FenceHolder(const FenceHolder&) = default;
        FenceHolder(FenceHolder&&) noexcept = default;
        FenceHolder& operator=(FenceHolder replacement) noexcept {
            swap(replacement);
            return *this;
        }
        void swap(FenceHolder& other) noexcept { _owner.swap(other._owner); }
        explicit operator bool() const noexcept { return bool(_owner); }

        std::size_t size() const;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX);
        bool wait(std::uint32_t target_idx, std::uint64_t timeout_ns = UINT64_MAX);
        void reset();
        void reset(std::uint32_t target_idx);
        void destroy();
    };

    struct SynchroManager {
    private:
        friend struct lifetime::detail::SynchronizationAccess;
        std::shared_ptr<impl::SynchroManagerImpl> _registry;

    public:
        SynchroManager() = default;
        SynchroManager(const SynchroManager&) = default;
        SynchroManager(SynchroManager&&) noexcept = default;
        SynchroManager& operator=(SynchroManager replacement) noexcept {
            swap(replacement);
            return *this;
        }
        void swap(SynchroManager& other) noexcept { _registry.swap(other._registry); }
        explicit operator bool() const noexcept { return bool(_registry); }
        LogicalDeviceAccessor get_device() const;

        SynchroManager(LogicalDeviceAccessor device);

        SemaphoreHolder create_semaphore(std::string name);
        SemaphoreHolder create_semaphore(std::string name, size_t amount);
        FenceHolder create_fence(std::string name, bool signal = false);
        FenceHolder create_fence(std::string name, size_t amount, bool signal = false);

        std::optional<SemaphoreHolder> get_semaphore(std::string name);
        std::optional<FenceHolder> get_fence(std::string name);

        bool destroy_semaphore(std::string name);
        bool destroy_fence(std::string name);

        std::optional<FenceHolder> operator[](std::string name);
    };
} // namespace VSL_NAMESPACE
