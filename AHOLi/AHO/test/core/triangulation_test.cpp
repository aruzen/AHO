#include <AHO/core/triangulation.hpp>
#include <gtest/gtest.h>
#include <array>
#include <forward_list>
#include <numbers>

namespace {
    using P = aho::d2::PointD;
    using E = aho::TriangulationFailedCause;

    template<typename T>
    class TriangulationCoordinateTest : public testing::Test {};

    using CalculationPoints = testing::Types<
        aho::d2::Point<long double>,
        aho::d2::Point<long double, aho::coordinate::MakeCoordinateInfo<true, false, true>::Value>,
        aho::d2::Point<long double, aho::coordinate::MakeCoordinateInfo<false, true, true>::Value>>;
    TYPED_TEST_SUITE(TriangulationCoordinateTest, CalculationPoints);

    TYPED_TEST(TriangulationCoordinateTest, PreservesLongDoubleCoordinatesAndTheirEnabledAxes) {
        const std::array points{TypeParam(0, 0), TypeParam(4, 0), TypeParam(4, 2), TypeParam(0, 2), TypeParam(0, 0)};
        const auto result = aho::triangulate(points);
        ASSERT_TRUE(result);
        static_assert(std::same_as<typename decltype(result)::value_type::CoordinateInfo, typename TypeParam::CoordinateInfo>);
        static_assert(std::same_as<typename decltype(result)::value_type::ElementType, long double>);
        EXPECT_EQ(result->size(), 2u);
        EXPECT_DOUBLE_EQ(result->area(), 8);
        ASSERT_EQ(result->points().size(), 4u);
        for (std::size_t i = 0; i < result->points().size(); ++i) {
            EXPECT_EQ(result->points()[i].value.___AN1, points[i].value.___AN1);
            EXPECT_EQ(result->points()[i].value.___AN2, points[i].value.___AN2);
        }
    }

    TEST(TriangulationTest, TriangleAndSquareKeepInputVerticesAndWinding) {
        for (const bool reverse : {false, true}) {
            std::vector<P> points{P(0, 0), P(2, 0), P(2, 3), P(0, 3)};
            if (reverse)
                std::ranges::reverse(points);
            auto result = aho::triangulate(points);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->points().size(), 4u);
            EXPECT_EQ(result->size(), 2u);
            EXPECT_DOUBLE_EQ(result->area(), 6);
            for (const auto triangle : result->triangles()) {
                const auto& a = triangle.pos1.value;
                const auto& b = triangle.pos2.value;
                const auto& c = triangle.pos3.value;
                const auto cross = (b.___AN1 - a.___AN1) * (c.___AN2 - a.___AN2) - (b.___AN2 - a.___AN2) * (c.___AN1 - a.___AN1);
                EXPECT_EQ(cross < 0, reverse);
            }
        }
        const std::array triangle{P(0, 0), P(3, 0), P(0, 4)};
        auto result = aho::triangulate(triangle);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->size(), 1u);
        EXPECT_EQ(result->area(), 6);
    }
    TEST(TriangulationTest, ConcaveContourProducesOnlyInteriorTriangles) {
        const std::array points{P(0, 0), P(3, 0), P(3, 1), P(1, 1), P(1, 3), P(0, 3)};
        auto result = aho::triangulate(points);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->size(), 4u);
        EXPECT_DOUBLE_EQ(result->area(), 5);
        for (const auto t : result->triangles()) {
            const double x = (t.pos1.value.___AN1 + t.pos2.value.___AN1 + t.pos3.value.___AN1) / 3;
            const double y = (t.pos1.value.___AN2 + t.pos2.value.___AN2 + t.pos3.value.___AN2) / 3;
            EXPECT_TRUE(x <= 1 || y <= 1);
        }
    }
    TEST(TriangulationTest, AcceptsClosedNonRandomAccessContourAndCollinearBoundaryPoints) {
        const std::forward_list<P> points{P(0, 0), P(1, 0), P(2, 0), P(2, 2), P(0, 2), P(0, 0)};
        auto result = aho::triangulate(points);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->points().size(), 5u);
        EXPECT_EQ(result->size(), 2u);
        EXPECT_DOUBLE_EQ(result->area(), 4);
    }
    TEST(TriangulationTest, ReportsInvalidInputsWithSpecificCauses) {
        auto expect = [](std::vector<P> points, E error) {
            const auto result = aho::triangulate(points);
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error(), error);
        };
        expect({}, E::TOO_FEW_VERTICES);
        expect({P(0, 0), P(1, 0)}, E::TOO_FEW_VERTICES);
        expect({P(0, 0), P(1, 1), P(1, 1), P(0, 1)}, E::DUPLICATE_VERTEX);
        expect({P(0, 0), P(1, 0), P(2, 0)}, E::DEGENERATE_CONTOUR);
        expect({P(0, 0), P(2, 2), P(0, 2), P(2, 0)}, E::SELF_INTERSECTION);
        expect({P(0, 0), P(INFINITY, 0), P(0, 1)}, E::NON_FINITE_COORDINATE);
        expect({P(0, 0), P(1, NAN), P(0, 1)}, E::NON_FINITE_COORDINATE);
    }
    TEST(TriangulationTest, NormalizationPreservesVeryLargeAndVerySmallContours) {
        for (const auto scale : {1e-150, 1e150}) {
            const std::array points{P(0, 0), P(scale, 0), P(scale, scale), P(0, scale)};
            auto result = aho::triangulate(points);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->size(), 2u);
            EXPECT_NEAR(result->area() / (scale * scale), 1, 1e-12);
        }
    }
    TEST(TriangulationTest, SupportsIntegerCoordinatesAndCustomIndexType) {
        const std::array points{aho::d2::PointI(-2, -2), aho::d2::PointI(2, -2), aho::d2::PointI(2, 2), aho::d2::PointI(-2, 2)};
        auto result = aho::triangulate<std::uint8_t>(points);
        ASSERT_TRUE(result);
        static_assert(std::same_as<typename decltype(result)::value_type::IndexType, std::uint8_t>);
        EXPECT_DOUBLE_EQ(result->area(), 16);
    }
    TEST(TriangulationTest, ConvexContoursPreserveAreaForDeterministicVertexCounts) {
        for (std::size_t n = 3; n <= 30; ++n) {
            std::vector<P> points;
            for (std::size_t i = 0; i < n; ++i) {
                const double angle = 2 * std::numbers::pi * i / n;
                points.emplace_back(std::cos(angle), std::sin(angle));
            }
            auto result = aho::triangulate(points);
            ASSERT_TRUE(result) << n;
            EXPECT_EQ(result->size(), n - 2);
            const double expected = n * std::sin(2 * std::numbers::pi / n) / 2;
            EXPECT_NEAR(result->area(), expected, 1e-12);
        }
    }
    TEST(TriangulationTest, AcceptsPointObjectsConvertibleToTheExistingPointType) {
        struct PointObject {
            using ElementType = double;
            using Dimension = vsl::D2;
            using CoordinateInfo = P::CoordinateInfo;
            P point;
            operator P() const { return point; }
        };
        const std::array points{PointObject{P(0, 0)}, PointObject{P(3, 0)}, PointObject{P(0, 4)}};
        auto result = aho::triangulate(points);
        ASSERT_TRUE(result);
        EXPECT_DOUBLE_EQ(result->area(), 6);
    }
    TEST(TriangulationTest, RejectsIndexOverflowBeforeConversion) {
        std::vector<P> points;
        for (int i = 0; i < 257; ++i)
            points.emplace_back(i, i % 3);
        const auto result = aho::triangulate<std::uint8_t>(points);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), E::INDEX_OVERFLOW);
    }
} // namespace
