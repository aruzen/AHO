#pragma once
#include <compare>

namespace aho_test {
    struct CastTarget {
        int value{};
        auto operator<=>(const CastTarget&) const = default;
    };
    struct CastSource {
        int value{};
        constexpr explicit operator CastTarget() const { return {value}; }
        auto operator<=>(const CastSource&) const = default;
    };
    template<class From, class To>
    concept HasCast = requires(From& value) { value.template cast<To>(); };
} // namespace aho_test
