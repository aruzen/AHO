#pragma once
#include "../define.hpp"
#include <cstddef>
#include <cstdint>
#include <compare>

namespace VSL_NAMESPACE::typed {
    struct ResourceId {
        std::uint64_t value;
        constexpr auto operator<=>(const ResourceId&) const = default;
    };
    enum class LifetimeScope {
        SHARED,
        FRAME,
        TRANSIENT
    };
    struct LifetimeGroup {
        LifetimeScope scope;
        size_t instance;
        constexpr auto operator<=>(const LifetimeGroup&) const = default;
    };
} // namespace VSL_NAMESPACE::typed
