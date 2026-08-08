#include <aho/core/math/mat.hpp>

#include <gtest/gtest.h>

TEST(AHOMatrix, AddsMatrices)
{
    const aho::Mat<int, 2, 2> lhs{{{1, 2}, {3, 4}}};
    const aho::Mat<int, 2, 2> rhs{{{10, 20}, {30, 40}}};

    const auto result = lhs + rhs;

    EXPECT_EQ(result[0][0], 11);
    EXPECT_EQ(result[0][1], 22);
    EXPECT_EQ(result[1][0], 33);
    EXPECT_EQ(result[1][1], 44);
}

TEST(AHOMatrix, MultipliesMatrices)
{
    const aho::Mat<int, 2, 3> lhs{{{1, 2, 3}, {4, 5, 6}}};
    const aho::Mat<int, 3, 2> rhs{{{7, 8}, {9, 10}, {11, 12}}};

    const auto result = lhs * rhs;

    EXPECT_EQ(result[0][0], 58);
    EXPECT_EQ(result[0][1], 64);
    EXPECT_EQ(result[1][0], 139);
    EXPECT_EQ(result[1][1], 154);
}

TEST(AHOMatrix, CreatesIdentityMatrix)
{
    const auto identity = aho::matrix::make_identity<int, 3>();

    EXPECT_EQ(identity[0][0], 1);
    EXPECT_EQ(identity[1][1], 1);
    EXPECT_EQ(identity[2][2], 1);
    EXPECT_EQ(identity[0][1], 0);
    EXPECT_EQ(identity[1][2], 0);
}
