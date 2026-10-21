#pragma once
#include <gtest/gtest.h>
#include <AHO/core/math/mat.hpp>
#include <array>
#include <cmath>
#include <limits>

namespace aho_test {
    template<class T, std::size_t rows, std::size_t columns>
    aho::Mat<T, rows, columns> matrix(const std::array<std::array<T, columns>, rows>& values) {
        return aho::Mat<T, rows, columns>(values);
    }

    template<class T, class U, std::size_t rows, std::size_t columns>
    void expect_matrix(const aho::Mat<T, rows, columns>& actual,
                       const std::array<std::array<U, columns>, rows>& expected) {
        // Small fixed matrices use 128 rounding units, scaled to the expected magnitude.
        constexpr double epsilon = std::is_floating_point_v<T> ? std::numeric_limits<T>::epsilon() : 0.0;
        for (std::size_t row = 0; row < rows; ++row) {
            for (std::size_t column = 0; column < columns; ++column) {
                SCOPED_TRACE(testing::Message() << "row=" << row << ", column=" << column);
                const double value = static_cast<double>(expected[row][column]);
                EXPECT_NEAR(static_cast<double>(actual[row][column]), value, 128 * epsilon * std::max(1.0, std::abs(value)));
            }
        }
    }

    template<class T, std::size_t size>
    void expect_identity(const aho::Mat<T, size, size>& matrix) {
        std::array<std::array<T, size>, size> expected{};
        for (std::size_t i = 0; i < size; ++i)
            expected[i][i] = T{1};
        expect_matrix(matrix, expected);
    }
} // namespace aho_test
