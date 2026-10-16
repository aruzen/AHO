#pragma once
#include <gtest/gtest.h>
#include <AHO/core/vector.hpp>
#include <AHO/core/point.hpp>
#include <array>
#include <limits>

namespace aho_test {
template<class T, class D> struct GeometryCase {
    using element_type = T;
    using dimension = D;
    using Vector = aho::_Vector<T, D>;
    using Point = aho::_Point<T, D>;
};
using GeometryCases = testing::Types<
    GeometryCase<int, vsl::D1>, GeometryCase<int, vsl::D2>, GeometryCase<int, vsl::D3>,
    GeometryCase<float, vsl::D1>, GeometryCase<float, vsl::D2>, GeometryCase<float, vsl::D3>,
    GeometryCase<double, vsl::D1>, GeometryCase<double, vsl::D2>, GeometryCase<double, vsl::D3>>;

template<class V> V geometry(double x, double y = 0, double z = 0) {
    using T = typename V::element_type;
    if constexpr (V::dimension::dimension == 1) return V(static_cast<T>(x));
    else if constexpr (V::dimension::dimension == 2) return V(static_cast<T>(x), static_cast<T>(y));
    else return V(static_cast<T>(x), static_cast<T>(y), static_cast<T>(z));
}
template<class V> void expect_coordinates(const V& value, double x, double y = 0, double z = 0) {
    using T = typename V::element_type;
    // Coordinates in these cases have magnitude <= 100 and use at most a few operations.
    constexpr double epsilon = std::is_floating_point_v<T> ? 128 * std::numeric_limits<T>::epsilon() : 0.0;
    EXPECT_NEAR(value.value.x.value, x, epsilon * std::max(1.0, std::abs(x)));
    if constexpr (V::dimension::dimension >= 2)
        EXPECT_NEAR(value.value.y.value, y, epsilon * std::max(1.0, std::abs(y)));
    if constexpr (V::dimension::dimension >= 3)
        EXPECT_NEAR(value.value.z.value, z, epsilon * std::max(1.0, std::abs(z)));
}
}
