#pragma once
#include <compare>

namespace aho_test {
    struct UnaryScalar;
    struct UnaryResult {
        int value;
        constexpr operator UnaryScalar() const;
    };
    struct UnaryScalar {
        int value;
        constexpr explicit UnaryScalar(int value) : value(value) {}
        // Direct conversion chooses this constructor; implicit argument conversion uses UnaryResult's operator.
        constexpr explicit UnaryScalar(UnaryResult result) : value(result.value + 100) {}
        constexpr auto operator<=>(const UnaryScalar&) const = default;
    };
    constexpr UnaryResult operator-(const UnaryScalar& source) {
        return {-source.value};
    }
    constexpr UnaryResult::operator UnaryScalar() const {
        return UnaryScalar(value);
    }
} // namespace aho_test
