#pragma once
#include <VSL/lifetime/surface_recovery.hpp>
#include <memory>

namespace VSL_NAMESPACE::lifetime {
    struct CommandDomain;
    struct AcquireTicket;
    void recover_command_domain(CommandDomain& domain);
    void collect_acquire_scope(AcquireTicket& ticket, bool wait = false) noexcept;
} // namespace VSL_NAMESPACE::lifetime
