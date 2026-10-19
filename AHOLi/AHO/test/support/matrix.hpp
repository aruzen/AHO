#pragma once
#include <gtest/gtest.h>
#include <AHO/core/math/mat.hpp>
#include <array>
#include <cmath>
#include <limits>

namespace aho_test {
    template<class T, std::size_t Rows, std::size_t Columns>
    aho::Mat<T, Rows, Columns> matrix(const std::array<std::array<T, Columns>, Rows>& values) {
        return aho::Mat<T, Rows, Columns>(values);
    }

    template<class T, class U, std::size_t Rows, std::size_t Columns>
    void expect_matrix(const aho::Mat<T, Rows, Columns>& actual,
                       const std::array<std::array<U, Columns>, Rows>& expected) {
        // Small fixed matrices use 128 rounding units, scaled to the expected magnitude.
        constexpr double epsilon = std::is_floating_point_v<T> ? std::numeric_limits<T>::epsilon() : 0.0;
        for (std::size_t row = 0; row < Rows; ++row) {
            for (std::size_t column = 0; column < Columns; ++column) {
                SCOPED_TRACE(testing::Message() << "row=" << row << ", column=" << column);
                const double value = static_cast<double>(expected[row][column]);
                EXPECT_NEAR(static_cast<double>(actual[row][column]), value, 128 * epsilon * std::max(1.0, std::abs(value)));
            }
        }
    }

    template<class T, std::size_t Size>
    void expect_identity(const aho::Mat<T, Size, Size>& matrix) {
        std::array<std::array<T, Size>, Size> expected{};
        for (std::size_t i = 0; i < Size; ++i)
            expected[i][i] = T{1};
        expect_matrix(matrix, expected);
    }
} // namespace aho_test
