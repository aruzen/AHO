#include <AHO/core/collision.hpp>
#include <AHO/core/triangulation.hpp>
#include <gtest/gtest.h>
#include <array>

namespace {
    using P = aho::d2::PointD;
    using T = aho::Triangle<double, vsl::D2>;
    auto rectangle(double x, double y, double w, double h) {
        return aho::triangulate(std::array{P(x, y), P(x + w, y), P(x + w, y + h), P(x, y + h)}).value();
    }
    TEST(CollisionTest, ContainsInteriorEdgesAndVerticesButNotExterior) {
        const auto shape = rectangle(0, 0, 2, 2);
        for (const auto p : {P(1, 1), P(0, 0), P(2, 1), P(1, 0)})
            EXPECT_TRUE(aho::collision::contains(shape, p));
        for (const auto p : {P(-1, 1), P(3, 1), P(1, 3)})
            EXPECT_FALSE(aho::collision::contains(shape, p));
        const T triangle{P(0, 0), P(2, 0), P(0, 2)};
        EXPECT_TRUE(aho::collision::contains(triangle, P(0.5, 0.5)));
        EXPECT_FALSE(aho::collision::contains(triangle, P(1.5, 1.5)));
    }
    TEST(CollisionTest, ConcaveNotchIsOutside) {
        const auto shape = aho::triangulate(std::array{P(0, 0), P(3, 0), P(3, 1), P(1, 1), P(1, 3), P(0, 3)}).value();
        EXPECT_TRUE(aho::collision::contains(shape, P(0.5, 2)));
        EXPECT_FALSE(aho::collision::contains(shape, P(2, 2)));
        EXPECT_FALSE(aho::collision::intersects(shape, rectangle(1.5, 1.5, 0.5, 0.5)));
        EXPECT_TRUE(aho::collision::intersects(shape, rectangle(0.5, 1.5, 0.5, 0.5)));
    }
    TEST(CollisionTest, IntersectionsIncludeContainmentEdgesAndVertexContact) {
        const auto shape = rectangle(0, 0, 2, 2);
        for (const auto other : {rectangle(1, 1, 2, 2), rectangle(0.5, 0.5, 0.5, 0.5), rectangle(2, 0, 1, 1), rectangle(2, 2, 1, 1), shape}) {
            EXPECT_TRUE(aho::collision::intersects(shape, other));
            EXPECT_TRUE(aho::collision::intersects(other, shape));
            EXPECT_TRUE(aho::collision::CollisionJudger{}(shape, other));
        }
        EXPECT_FALSE(aho::collision::intersects(shape, rectangle(2.01, 0, 1, 1)));
    }
    TEST(CollisionTest, CrossingTrianglesNeedNoContainedVertex) {
        const T a{P(-3, -1), P(3, -1), P(0, 3)};
        const T b{P(-3, 1), P(3, 1), P(0, -3)};
        EXPECT_TRUE(aho::collision::intersects(a, b));
    }
    TEST(CollisionTest, EmptyMeshesNeverIntersectOrContain) {
        const aho::Polygon<double, vsl::D2> empty;
        const auto shape = rectangle(0, 0, 2, 2);
        EXPECT_FALSE(aho::collision::contains(empty, P(0, 0)));
        EXPECT_FALSE(aho::collision::intersects(empty, shape));
        EXPECT_FALSE(aho::collision::intersects(shape, empty));
        EXPECT_FALSE(aho::collision::intersects(empty, empty));
    }
    TEST(CollisionTest, DegenerateTrianglesBehaveAsClosedSegmentsAndPoints) {
        const T segment{P(0, 0), P(1, 0), P(2, 0)};
        const T point{P(1, 0), P(1, 0), P(1, 0)};
        EXPECT_TRUE(aho::collision::contains(segment, P(1, 0)));
        EXPECT_FALSE(aho::collision::contains(segment, P(1, 1)));
        EXPECT_TRUE(aho::collision::intersects(segment, point));
        EXPECT_TRUE(aho::collision::intersects(point, point));
        EXPECT_FALSE(aho::collision::intersects(segment, T{P(0, 1), P(1, 1), P(2, 1)}));
    }
    TEST(CollisionTest, MixedScalarTypesKeepTheSameCoordinateContract) {
        const aho::Triangle<int, vsl::D2> integer{aho::d2::PointI(0, 0), aho::d2::PointI(4, 0), aho::d2::PointI(0, 4)};
        EXPECT_TRUE(aho::collision::contains(integer, P(0.5, 0.5)));
        EXPECT_TRUE(aho::collision::intersects(integer, rectangle(0.5, 0.5, 0.5, 0.5)));
    }
    TEST(CollisionTest, HandlesLargeAndSmallScales) {
        for (const double s : {1e-150, 1e150}) {
            const auto shape = rectangle(0, 0, s, s);
            EXPECT_TRUE(aho::collision::contains(shape, P(s / 2, s / 2)));
            EXPECT_FALSE(aho::collision::contains(shape, P(s * 2, s / 2)));
            EXPECT_TRUE(aho::collision::intersects(shape, rectangle(s / 2, s / 2, s, s)));
        }
    }
    TEST(CollisionTest, NonFiniteInputsAreRejectedRatherThanReportedAsMisses) {
        const auto shape = rectangle(0, 0, 1, 1);
        const T invalid{P(0, 0), P(INFINITY, 0), P(0, 1)};
        EXPECT_THROW(aho::collision::contains(shape, P(NAN, 0)), std::invalid_argument);
        EXPECT_THROW(aho::collision::contains(invalid, P(0, 0)), std::invalid_argument);
        EXPECT_THROW(aho::collision::intersects(invalid, shape), std::invalid_argument);
        EXPECT_THROW(aho::collision::intersects(shape, invalid), std::invalid_argument);
    }
} // namespace
