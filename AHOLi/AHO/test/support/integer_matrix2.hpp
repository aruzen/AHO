#pragma once
#include <compare>

namespace aho_test {
    struct IntegerMatrix2 {
        int a, b, c, d;
        constexpr auto operator<=>(const IntegerMatrix2&) const = default;
    };
    constexpr IntegerMatrix2 operator*(const IntegerMatrix2& lhs, const IntegerMatrix2& rhs) {
        return {lhs.a * rhs.a + lhs.b * rhs.c, lhs.a * rhs.b + lhs.b * rhs.d,
                lhs.c * rhs.a + lhs.d * rhs.c, lhs.c * rhs.b + lhs.d * rhs.d};
    }
} // namespace aho_test
