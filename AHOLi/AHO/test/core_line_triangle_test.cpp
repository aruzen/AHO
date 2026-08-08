#include <aho/core/line.hpp>
#include <aho/core/triangle.hpp>

#include <gtest/gtest.h>

namespace {
using Coordinate2D = aho::coordinate::_DefaultCoordinateInfo<vsl::D2>::value;
using Point2D = aho::_Point<double, vsl::D2, Coordinate2D>;
using Line2D = aho::Line<double, vsl::D2, Coordinate2D>;
using Triangle2D = aho::Triangle<double, vsl::D2, Coordinate2D>;
}

TEST(AHOLine, ReturnsPointsAndLines)
{
    const Line2D line{Point2D{1.0, 2.0}, Point2D{3.0, 4.0}};

    const auto points = line.points();
    const auto lines = line.lines();

    EXPECT_DOUBLE_EQ(points[0].value.___AN1, 1.0);
    EXPECT_DOUBLE_EQ(points[0].value.___AN2, 2.0);
    EXPECT_DOUBLE_EQ(points[1].value.___AN1, 3.0);
    EXPECT_DOUBLE_EQ(points[1].value.___AN2, 4.0);
    EXPECT_DOUBLE_EQ(lines[0].length(), line.length());
}

TEST(AHOLine, CreatesTriangleByAddingPoint)
{
    const Line2D line{Point2D{0.0, 0.0}, Point2D{4.0, 0.0}};

    const auto triangle = line + Point2D{0.0, 3.0};

    EXPECT_DOUBLE_EQ(triangle.area(), 6.0);
}

TEST(AHOLine, PointerAddressReflectsOriginalPoints)
{
    Line2D line{Point2D{1.0, 2.0}, Point2D{3.0, 4.0}};

    const auto ptr = line.adress();
    ptr.pos1->value.___AN1 = 10.0;

    EXPECT_DOUBLE_EQ(line.pos1.value.___AN1, 10.0);
    EXPECT_DOUBLE_EQ(ptr.indirect().length(), line.length());
}

TEST(AHOTriangle, ReturnsAreaAndPoints)
{
    const Triangle2D triangle{Point2D{0.0, 0.0}, Point2D{4.0, 0.0}, Point2D{0.0, 3.0}};

    const auto points = triangle.points();

    EXPECT_DOUBLE_EQ(triangle.area(), 6.0);
    EXPECT_DOUBLE_EQ(points[0].value.___AN1, 0.0);
    EXPECT_DOUBLE_EQ(points[1].value.___AN1, 4.0);
    EXPECT_DOUBLE_EQ(points[2].value.___AN2, 3.0);
}

TEST(AHOTriangle, PointerAddressReflectsOriginalPoints)
{
    Triangle2D triangle{Point2D{0.0, 0.0}, Point2D{4.0, 0.0}, Point2D{0.0, 3.0}};

    const auto ptr = triangle.adress();
    ptr.pos3->value.___AN2 = 6.0;

    EXPECT_DOUBLE_EQ(triangle.pos3.value.___AN2, 6.0);
    EXPECT_DOUBLE_EQ(ptr.area(), 12.0);
}
