#include <aho/core/line.hpp>
#include <aho/core/point.hpp>
#include <aho/core/vector.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace {
using Coordinate2D = aho::coordinate::_DefaultCoordinateInfo<vsl::D2>::value;
using Point2D = aho::_Point<double, vsl::D2, Coordinate2D>;
using Vector2D = aho::_Vector<double, vsl::D2, Coordinate2D>;
}

TEST(AHOPointVector, SubtractsPointsIntoVector)
{
    const Point2D lhs{5.0, 7.0};
    const Point2D rhs{2.0, 3.0};

    const auto result = lhs - rhs;

    EXPECT_DOUBLE_EQ(result.value.___AN1, 3.0);
    EXPECT_DOUBLE_EQ(result.value.___AN2, 4.0);
}

TEST(AHOPointVector, AddsVectorToPoint)
{
    const Point2D point{2.0, 3.0};
    const Vector2D vec{4.0, 5.0};

    const auto result = point + vec;

    EXPECT_DOUBLE_EQ(result.value.___AN1, 6.0);
    EXPECT_DOUBLE_EQ(result.value.___AN2, 8.0);
}

TEST(AHOPointVector, CalculatesVectorProducts)
{
    const Vector2D lhs{3.0, 4.0};
    const Vector2D rhs{10.0, 20.0};

    const auto hadamard = lhs.hadamard(rhs);
    const auto cross = lhs.cross(rhs);

    EXPECT_DOUBLE_EQ(lhs.dot(rhs), 110.0);
    EXPECT_DOUBLE_EQ(hadamard.value.___AN1, 30.0);
    EXPECT_DOUBLE_EQ(hadamard.value.___AN2, 80.0);
    EXPECT_DOUBLE_EQ(cross.value.___AN1, 20.0);
    EXPECT_DOUBLE_EQ(lhs.length(), 5.0);
}

TEST(AHOPointVector, CalculatesLineLength)
{
    const aho::Line<double, vsl::D2, Coordinate2D> line{Point2D{0.0, 0.0}, Point2D{3.0, 4.0}};

    EXPECT_DOUBLE_EQ(line.length(), 5.0);
}
