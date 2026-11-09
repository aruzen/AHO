#pragma once
#include <VSL/lifetime/surface_recovery.hpp>
#include <memory>

namespace VSL_NAMESPACE::impl {
    struct LogicalDeviceImpl;
}

namespace VSL_NAMESPACE::lifetime {
    struct CommandDomain;
    struct AcquireTicket;
    void recover_command_domain(CommandDomain& domain);
    void collect_acquire_scope(AcquireTicket& ticket, bool wait = false) noexcept;
    // Native結果と取消状態をcommitする外側scopeに置く。Native guard自身からは回収しない。
    class SurfaceRetirementCollection {
        std::shared_ptr<impl::LogicalDeviceImpl> _device;

    public:
        explicit SurfaceRetirementCollection(std::shared_ptr<impl::LogicalDeviceImpl> device) noexcept;
        ~SurfaceRetirementCollection();
        SurfaceRetirementCollection(const SurfaceRetirementCollection&) = delete;
        SurfaceRetirementCollection& operator=(const SurfaceRetirementCollection&) = delete;
    };
} // namespace VSL_NAMESPACE::lifetime
