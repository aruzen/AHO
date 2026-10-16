#include <AHO/core/triangle.hpp>
#include <AHO/utils/polygon.hpp>
#include <gtest/gtest.h>
#if defined(AHO_CONTRACT_POLYGON_LINES_VIEW)
struct LinesObject {
    using element_type = double; using dimension = vsl::D2;
    using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
    auto lines() { return std::array<aho::Line<double, vsl::D2>, 1>{aho::Line<double, vsl::D2>(aho::d2::PointD(0, 0), aho::d2::PointD(3, 4))}; }
    auto points() { return std::array<aho::d2::PointD, 2>{}; }
};
TEST(PolygonContractTest, CompositeLineViewUsesLinesRatherThanPoints) {
    LinesObject value;
    const auto result = aho::views::lines(value);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(result)>, aho::Line<double, vsl::D2>>);
    ASSERT_EQ(std::ranges::distance(result), 1);
    EXPECT_DOUBLE_EQ(result.begin()->length(), 5);
}
#elif defined(AHO_CONTRACT_POLYGON_TRIANGLES_VIEW)
struct TrianglesObject {
    using element_type = double; using dimension = vsl::D2;
    using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
    auto triangles() { return std::array<aho::Triangle<double, vsl::D2>, 1>{aho::Triangle<double, vsl::D2>(aho::d2::PointD(0, 0), aho::d2::PointD(3, 0), aho::d2::PointD(0, 4))}; }
    auto points() { return std::array<aho::d2::PointD, 3>{}; }
};
TEST(PolygonContractTest, CompositeTriangleViewUsesTrianglesRatherThanPoints) {
    TrianglesObject value;
    const auto result = aho::views::triangles(value);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(result)>, aho::Triangle<double, vsl::D2>>);
    ASSERT_EQ(std::ranges::distance(result), 1);
    EXPECT_DOUBLE_EQ(result.begin()->area(), 6);
}
#else
#include "../support/geometry.hpp"
namespace {
TEST(PolygonViewsTest, SinglePointLineAndTriangleProduceSingleElementViews) {
    aho::d2::PointD point(2, 3);
    auto points = aho::views::points(point);
    EXPECT_EQ(std::ranges::distance(points), 1);
    aho_test::expect_coordinates(*points.begin(), 2, 3);
    aho::Line<double, vsl::D2> line(point, aho::d2::PointD(5, 7));
    auto lines = aho::views::lines(line);
    EXPECT_EQ(std::ranges::distance(lines), 1); EXPECT_DOUBLE_EQ(lines.begin()->length(), 5);
    auto triangle = line + aho::d2::PointD(5, 3);
    auto triangles = aho::views::triangles(triangle);
    EXPECT_EQ(std::ranges::distance(triangles), 1); EXPECT_DOUBLE_EQ(triangles.begin()->area(), 6);
}
template<bool X, bool Y, bool Z> struct PointViewCase {
    using Info = aho::coordinate::_MakeCoordinateInfo<X, Y, Z>::value;
    using Point = aho::_Point<double, typename Info::dimension, Info>;
};
using PointViewMasks = testing::Types<PointViewCase<true, false, false>, PointViewCase<false, true, false>,
    PointViewCase<false, false, true>, PointViewCase<true, true, false>, PointViewCase<true, false, true>,
    PointViewCase<false, true, true>, PointViewCase<true, true, true>>;
template<class T> class PointViewCoordinateTest : public testing::Test {};
TYPED_TEST_SUITE(PointViewCoordinateTest, PointViewMasks);
TYPED_TEST(PointViewCoordinateTest, CompositePointSnapshotsKeepTheOriginalCoordinateAxes) {
    using CI = typename TypeParam::Info;
    using P = typename TypeParam::Point;
    aho::Line<double, typename CI::dimension, CI> line(aho_test::geometry<P>(1, 2, 3), aho_test::geometry<P>(4, 5, 6));
    const auto& source = line;
    const auto points = aho::views::points(source);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(points)>, P>);
    ASSERT_EQ(points.size(), 2u);
    auto check = [](const P& point, double first) {
        if constexpr (CI::EnabledX) EXPECT_EQ(point.value.x.value, first);
        if constexpr (CI::EnabledY) EXPECT_EQ(point.value.y.value, first + CI::EnabledX);
        if constexpr (CI::EnabledZ) EXPECT_EQ(point.value.z.value, first + CI::EnabledX + CI::EnabledY);
    };
    check(points[0], 1); check(points[1], 4);
    line.pos1 = aho_test::geometry<P>(7, 8, 9);
    check(points[0], 1);
    check(aho::views::points(line)[0], 7);
}
TEST(PolygonViewsTest, CompositePointViewCopiesEveryVertex) {
    aho::Line<double, vsl::D2> line(aho::d2::PointD(1, 2), aho::d2::PointD(3, 4));
    const auto points = aho::views::points(line);
    ASSERT_EQ(points.size(), 2u);
    aho_test::expect_coordinates(points[0], 1, 2); aho_test::expect_coordinates(points[1], 3, 4);
}
TEST(PolygonViewsTest, CastAndWrapViewsPreserveOrderAndEmptyRanges) {
    std::array<double, 3> values{1.5, 2.5, 3.5};
    EXPECT_EQ(aho::views::cast<int>(values), (std::vector<int>{1, 2, 3}));
    std::vector<double> empty;
    EXPECT_TRUE(aho::views::cast<int>(empty).empty());
    aho::views::wrap_view view(std::views::all(values));
    EXPECT_EQ(std::ranges::distance(view.begin(), view.end()), 3);
    *view.begin() = 9;
    EXPECT_DOUBLE_EQ(values[0], 9);
}
TEST(PolygonConceptsTest, UnrelatedTypesWithMatchingAliasesAreNotGeometricObjects) {
    struct PretendShape {
        using dimension = vsl::D2; using element_type = double;
        using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
    };
    EXPECT_FALSE(aho::concepts::is_line<PretendShape>);
    EXPECT_FALSE(aho::concepts::is_triangle<PretendShape>);
    EXPECT_FALSE(aho::concepts::points_view_able<PretendShape>);
}
}
#endif
