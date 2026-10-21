#pragma once
#include <AHO/core/math/coordinate.hpp>
#include <gtest/gtest.h>
#include <type_traits>

namespace aho_test::arithmetic {
    template<bool x, bool y, bool z>
    using ArithmeticCoordinates = typename aho::coordinate::MakeCoordinateInfo<x, y, z>::Value;
    using ArithmeticGeometryCoordinates = testing::Types<
        ArithmeticCoordinates<true, false, false>, ArithmeticCoordinates<false, true, false>,
        ArithmeticCoordinates<false, false, true>, ArithmeticCoordinates<true, true, false>,
        ArithmeticCoordinates<true, false, true>, ArithmeticCoordinates<false, true, true>,
        ArithmeticCoordinates<true, true, true>>;
    template<class V>
    constexpr V arithmetic_geometry(int first = 12, int second = 20, int third = 28) {
        using T = typename V::ElementType;
        if constexpr (V::Dimension::DIMENSION == 1)
            return V(T{first});
        else if constexpr (V::Dimension::DIMENSION == 2)
            return V(T{first}, T{second});
        else
            return V(T{first}, T{second}, T{third});
    }
    template<class V>
    constexpr bool arithmetic_values_are(const V& v, int first, int second, int third) {
        auto scalar = [](const auto& value) {
            if constexpr (std::is_arithmetic_v<std::remove_cvref_t<decltype(value)>>)
                return value;
            else
                return value.value;
        };
        bool result = scalar(v.value.___AN1) == first;
        if constexpr (V::Dimension::DIMENSION >= 2)
            result = result && scalar(v.value.___AN2) == second;
        if constexpr (V::Dimension::DIMENSION == 3)
            result = result && scalar(v.value.___AN3) == third;
        return result;
    }
} // namespace aho_test::arithmetic
