#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include <memory>
#include <map>
#include <optional>

#include "device.hpp"

namespace VSL_NAMESPACE {
    struct SemaphoreHolder {
        std::shared_ptr<impl::SemaphoreImpl> data_;

        std::size_t size() const;
        void destroy();
    };

    struct FenceHolder {
        std::shared_ptr<impl::FenceImpl> data_;

        bool wait(std::uint64_t timeout_ns = UINT64_MAX);
        bool wait(std::uint32_t target_idx, std::uint64_t timeout_ns = UINT64_MAX);
        void reset();
        void reset(std::uint32_t target_idx);
        void destroy();
    };

    struct SynchroManager {
        std::shared_ptr<impl::SynchroManagerImpl> data_;

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
