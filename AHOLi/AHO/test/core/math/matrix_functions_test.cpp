#include <AHO/core/math/matrix_functions.hpp>
#include <gtest/gtest.h>

namespace {
    TEST(MatrixFunctionsPolicyTest, InitializerAndArrayFactoriesDefaultToRows) {
        constexpr std::array<int, 3> rows[]{{1, 2, 3}, {4, 5, 6}};
        constexpr auto from_rows = aho::matrix::make(rows);
        constexpr auto from_lists = aho::matrix::make<int, 2, 3>({{1, 2, 3}, {4, 5, 6}});
        static_assert(from_rows == from_lists);
        constexpr auto from_columns = aho::matrix::make<int, 2, 3, aho::MatrixLayout::COLUMN_MAJOR>({{1, 4}, {2, 5}, {3, 6}});
        static_assert(from_columns == from_rows);
        EXPECT_EQ(from_columns, from_lists);
    }

    TEST(MatrixFunctionsPolicyTest, StorageOrderIsSeparateFromVectorConvention) {
        constexpr aho::Mat<int, 2, 3> value{{1, 2, 3}, {4, 5, 6}};
        constexpr auto rows = aho::matrix::to_array(value);
        constexpr auto columns = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(value);
        static_assert(rows == std::array{1, 2, 3, 4, 5, 6});
        static_assert(columns == std::array{1, 4, 2, 5, 3, 6});
        static_assert(aho::matrix::from_array<int, 2, 3>(rows) == value);
        static_assert(aho::matrix::from_array<int, 2, 3, aho::MatrixLayout::COLUMN_MAJOR>(columns) == value);
        EXPECT_NE(rows, columns);
    }
} // namespace
