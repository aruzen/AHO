#include <aho/core/math/angle.hpp>

#include <gtest/gtest.h>

#include <numbers>

TEST(AHOAngle, ConvertsDegreeToRadian)
{
    const aho::Radian rad = aho::Degree{180.0};

    EXPECT_NEAR(static_cast<double>(rad), std::numbers::pi, 1.0e-12);
}

TEST(AHOAngle, ConvertsRadianToDegree)
{
    const aho::Degree deg = aho::Radian{std::numbers::pi / 2.0};

    EXPECT_NEAR(static_cast<double>(deg), 90.0, 1.0e-12);
}

TEST(AHOAngle, SupportsArithmetic)
{
    aho::Degree deg{45.0};
    deg += aho::Degree{15.0};
    deg *= 2.0;

    EXPECT_DOUBLE_EQ(static_cast<double>(deg), 120.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(aho::Degree{90.0} / 3.0), 30.0);
}
