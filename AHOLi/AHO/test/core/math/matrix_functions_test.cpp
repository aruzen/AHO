#include <AHO/core/math/matrix_functions.hpp>
#include <gtest/gtest.h>

namespace {
    TEST(MatrixFunctionsPolicyTest, AffineInverseAcceptsBothVectorConventions) {
        const aho::Mat4x4<double> column{{2, 0, 0, 3}, {0, 4, 0, 5}, {0, 0, 8, 7}, {0, 0, 0, 1}};
        const auto row = aho::matrix::transpose(column);
        const auto a = aho::matrix::inverse_affine<aho::VectorConvention::COLUMN>(column);
        const auto b = aho::matrix::inverse_affine<aho::VectorConvention::ROW>(row);
        ASSERT_TRUE(a);
        ASSERT_TRUE(b);
        EXPECT_EQ(*b, aho::matrix::transpose(*a));
        const auto identity = row * *b;
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                EXPECT_NEAR(identity[r][c], r == c ? 1 : 0, 1e-12);
        auto singular = row;
        singular[0][0] = 0;
        const auto failed = aho::matrix::inverse_affine<aho::VectorConvention::ROW>(singular);
        ASSERT_FALSE(failed);
        EXPECT_EQ(failed.error(), aho::matrix::FailedCause::SINGULAR);
        auto non_affine = row;
        non_affine[0][3] = 1;
        const auto rejected = aho::matrix::inverse_affine<aho::VectorConvention::ROW>(non_affine);
        ASSERT_FALSE(rejected);
        EXPECT_EQ(rejected.error(), aho::matrix::FailedCause::NON_AFFINE);
    }

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
