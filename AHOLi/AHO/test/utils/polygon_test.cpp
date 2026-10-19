#include <AHO/core/triangle.hpp>
#include <AHO/utils/polygon.hpp>
#include <gtest/gtest.h>
#include <utility>
#include <forward_list>
#include <sstream>
#if defined(AHO_CONTRACT_POLYGON_LINES_VIEW)
struct LinesObject {
    using element_type = double;
    using dimension = vsl::D2;
    using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
    int points_calls = 0, lines_calls = 0;
    auto lines() {
        ++lines_calls;
        return std::array<aho::Line<double, vsl::D2>, 1>{aho::Line<double, vsl::D2>(aho::d2::PointD(0, 0), aho::d2::PointD(3, 4))};
    }
    auto points() {
        ++points_calls;
        return std::array<aho::d2::PointD, 2>{};
    }
};
TEST(PolygonContractTest, CompositeLineViewUsesLinesRatherThanPoints) {
    LinesObject value;
    const auto result = aho::views::lines(value);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(result)>, aho::Line<double, vsl::D2>>);
    ASSERT_EQ(std::ranges::distance(result), 1);
    EXPECT_DOUBLE_EQ(result.begin()->length(), 5);
    EXPECT_EQ(value.lines_calls, 1);
    EXPECT_EQ(value.points_calls, 0);
}
#elif defined(AHO_CONTRACT_POLYGON_TRIANGLES_VIEW)
struct TrianglesObject {
    using element_type = double;
    using dimension = vsl::D2;
    using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
    int points_calls = 0, triangles_calls = 0;
    auto triangles() {
        ++triangles_calls;
        return std::array<aho::Triangle<double, vsl::D2>, 1>{aho::Triangle<double, vsl::D2>(aho::d2::PointD(0, 0), aho::d2::PointD(3, 0), aho::d2::PointD(0, 4))};
    }
    auto points() {
        ++points_calls;
        return std::array<aho::d2::PointD, 3>{};
    }
};
TEST(PolygonContractTest, CompositeTriangleViewUsesTrianglesRatherThanPoints) {
    TrianglesObject value;
    const auto result = aho::views::triangles(value);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(result)>, aho::Triangle<double, vsl::D2>>);
    ASSERT_EQ(std::ranges::distance(result), 1);
    EXPECT_DOUBLE_EQ(result.begin()->area(), 6);
    EXPECT_EQ(value.triangles_calls, 1);
    EXPECT_EQ(value.points_calls, 0);
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
        EXPECT_EQ(std::ranges::distance(lines), 1);
        EXPECT_DOUBLE_EQ(lines.begin()->length(), 5);
        auto triangle = line + aho::d2::PointD(5, 3);
        auto triangles = aho::views::triangles(triangle);
        EXPECT_EQ(std::ranges::distance(triangles), 1);
        EXPECT_DOUBLE_EQ(triangles.begin()->area(), 6);
    }
    template<bool X, bool Y, bool Z>
    struct PointViewCase {
        using Info = aho::coordinate::_MakeCoordinateInfo<X, Y, Z>::value;
        using Point = aho::_Point<double, typename Info::dimension, Info>;
    };
    using PointViewMasks = testing::Types<PointViewCase<true, false, false>, PointViewCase<false, true, false>,
                                          PointViewCase<false, false, true>, PointViewCase<true, true, false>, PointViewCase<true, false, true>,
                                          PointViewCase<false, true, true>, PointViewCase<true, true, true>>;
    template<class T>
    class PointViewCoordinateTest : public testing::Test {};
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
            if constexpr (CI::EnabledX)
                EXPECT_EQ(point.value.x.value, first);
            if constexpr (CI::EnabledY)
                EXPECT_EQ(point.value.y.value, first + CI::EnabledX);
            if constexpr (CI::EnabledZ)
                EXPECT_EQ(point.value.z.value, first + CI::EnabledX + CI::EnabledY);
        };
        check(points[0], 1);
        check(points[1], 4);
        line.pos1 = aho_test::geometry<P>(7, 8, 9);
        check(points[0], 1);
        check(aho::views::points(line)[0], 7);
    }
    TEST(PolygonViewsTest, CompositePointViewCopiesEveryVertex) {
        aho::Line<double, vsl::D2> line(aho::d2::PointD(1, 2), aho::d2::PointD(3, 4));
        const auto points = aho::views::points(line);
        ASSERT_EQ(points.size(), 2u);
        aho_test::expect_coordinates(points[0], 1, 2);
        aho_test::expect_coordinates(points[1], 3, 4);
    }
    TEST(WrapViewTest, NonCommonRangesKeepTheirSentinelAndElementReferences) {
        std::array<int, 5> source{1, 2, 3, 0, 4};
        auto inner = std::views::take_while(source, [](int value) { return value > 0; });
        aho::views::wrap_view wrapped(inner);
        static_assert(!std::ranges::common_range<decltype(inner)>);
        static_assert(std::same_as<std::ranges::sentinel_t<decltype(wrapped)>, std::ranges::sentinel_t<decltype(inner)>>);
        std::vector<int> values;
        for (int value : wrapped)
            values.push_back(value);
        EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));
        *wrapped.begin() = 9;
        EXPECT_EQ(source[0], 9);
    }
    TEST(WrapViewTest, NonCommonRangesMayBeEmptyAtTheirFirstElement) {
        std::array<int, 2> source{0, 1};
        aho::views::wrap_view wrapped(std::views::take_while(source, [](int value) { return value > 0; }));
        EXPECT_EQ(wrapped.begin(), wrapped.end());
        EXPECT_TRUE(std::ranges::empty(wrapped));
    }
    TEST(WrapViewTest, MoveOnlyOwningViewsRetainTheirElementsAcrossMoves) {
        auto wrapped = aho::views::wrap_view(std::views::all(std::vector<int>{1, 2, 3}));
        static_assert(std::ranges::view<decltype(wrapped)>);
        static_assert(!std::copy_constructible<decltype(wrapped)>);
        auto moved = std::move(wrapped);
        EXPECT_EQ(std::ranges::distance(moved), 3);
        EXPECT_EQ((std::vector<int>(moved.begin(), moved.end())), (std::vector<int>{1, 2, 3}));
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
    TEST(PolygonCastTest, UnsizedForwardRangesPreserveOrderAndEmptyInputs) {
        const std::forward_list<double> values{1.5, -2.5, 3.5};
        static_assert(!std::ranges::sized_range<decltype(values)>);
        EXPECT_EQ(aho::views::cast<int>(values), (std::vector<int>{1, -2, 3}));
        EXPECT_TRUE(aho::views::cast<int>(std::forward_list<double>{}).empty());
    }
    TEST(PolygonCastTest, NonCommonRangesStopAtTheirSentinel) {
        std::array<double, 4> values{1.5, 2.5, 0.0, 9.5};
        auto prefix = values | std::views::take_while([](double value) { return value > 0; });
        static_assert(!std::ranges::common_range<decltype(prefix)>);
        EXPECT_EQ(aho::views::cast<int>(prefix), (std::vector<int>{1, 2}));
    }
    TEST(PolygonCastTest, SinglePassInputRangesAreConsumedOnce) {
        std::istringstream stream("1.5 -2.5 3.5");
        auto values = std::ranges::istream_view<double>(stream);
        static_assert(std::ranges::input_range<decltype(values)>);
        static_assert(!std::ranges::forward_range<decltype(values)>);
        EXPECT_EQ(aho::views::cast<int>(values), (std::vector<int>{1, -2, 3}));
        EXPECT_TRUE(stream.eof());
    }
    TEST(PolygonCastTest, ConvertedElementsDoNotRequireDefaultConstruction) {
        struct Converted {
            int value;
            Converted() = delete;
            explicit Converted(int value) : value(value) {}
        };
        const std::array<int, 2> source{3, 7};
        const auto converted = aho::views::cast<Converted>(source);
        ASSERT_EQ(converted.size(), 2u);
        EXPECT_EQ(converted[0].value, 3);
        EXPECT_EQ(converted[1].value, 7);
    }
    TEST(PolygonViewsTest, CompositeViewsAcceptUnsizedCollections) {
        using P = aho::d2::PointD;
        using L = aho::Line<double, vsl::D2>;
        using Triangle = aho::Triangle<double, vsl::D2>;
        struct Shape {
            using dimension = vsl::D2;
            using element_type = double;
            using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
            std::forward_list<P> vertices{P(0, 0), P(3, 4)};
            std::forward_list<L> edges{L(P(0, 0), P(3, 4))};
            std::forward_list<Triangle> faces{Triangle(P(0, 0), P(3, 0), P(0, 4))};
            const auto& points() const { return vertices; }
            const auto& lines() const { return edges; }
            const auto& triangles() const { return faces; }
        };
        static_assert(aho::concepts::is_points_object<Shape>);
        static_assert(aho::concepts::is_lines_object<Shape>);
        static_assert(aho::concepts::is_triangles_object<Shape>);
        Shape shape;
        const auto points = aho::views::points(shape);
        const auto lines = aho::views::lines(shape);
        const auto triangles = aho::views::triangles(shape);
        ASSERT_EQ(points.size(), 2u);
        ASSERT_EQ(lines.size(), 1u);
        ASSERT_EQ(triangles.size(), 1u);
        aho_test::expect_coordinates(points[0], 0, 0);
        aho_test::expect_coordinates(points[1], 3, 4);
        EXPECT_DOUBLE_EQ(lines[0].length(), 5);
        EXPECT_DOUBLE_EQ(triangles[0].area(), 6);
        shape.vertices.front() = P(9, 9);
        aho_test::expect_coordinates(points[0], 0, 0);
    }

    TEST(PolygonConceptsTest, UnrelatedTypesWithMatchingAliasesAreNotGeometricObjects) {
        struct PretendShape {
            using dimension = vsl::D2;
            using element_type = double;
            using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
        };
        EXPECT_FALSE(aho::concepts::is_line<PretendShape>);
        EXPECT_FALSE(aho::concepts::is_triangle<PretendShape>);
        EXPECT_FALSE(aho::concepts::points_view_able<PretendShape>);
    }
    TEST(PolygonViewsTest, CompositeViewsSupportEmptyCollectionsWithoutPointViews) {
        struct LinesOnly {
            using element_type = double;
            using dimension = vsl::D2;
            using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
            auto lines() { return std::array<aho::Line<double, vsl::D2>, 0>{}; }
        } lines;
        struct TrianglesOnly {
            using element_type = double;
            using dimension = vsl::D2;
            using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
            auto triangles() { return std::array<aho::Triangle<double, vsl::D2>, 0>{}; }
        } triangles;
        EXPECT_TRUE(aho::views::lines(lines).empty());
        EXPECT_TRUE(aho::views::triangles(triangles).empty());
    }

    template<class T>
    class DerivedShapeTest : public testing::Test {};
    TYPED_TEST_SUITE(DerivedShapeTest, aho_test::GeometryCases);
    TYPED_TEST(DerivedShapeTest, SingleShapeViewsPreserveTheDerivedTypeAndSnapshot) {
        using P = typename TypeParam::Point;
        using L = aho::Line<typename TypeParam::element_type, typename TypeParam::dimension>;
        using Triangle = aho::Triangle<typename TypeParam::element_type, typename TypeParam::dimension>;
        struct DerivedLine : L {
            using L::L;
            int marker = 17;
        };
        struct DerivedTriangle : Triangle {
            using Triangle::Triangle;
            int marker = 23;
        };
        static_assert(aho::concepts::is_line<DerivedLine>);
        static_assert(aho::concepts::is_line<const DerivedLine&>);
        static_assert(aho::concepts::is_triangle<DerivedTriangle>);
        static_assert(aho::concepts::is_triangle<const DerivedTriangle&>);
        DerivedLine line(aho_test::geometry<P>(1, 2, 3), aho_test::geometry<P>(4, 5, 6));
        const DerivedTriangle triangle(line.pos1, line.pos2, aho_test::geometry<P>(7, 8, 9));
        const auto lines = aho::views::lines(std::as_const(line));
        const auto triangles = aho::views::triangles(triangle);
        static_assert(std::same_as<std::ranges::range_value_t<decltype(lines)>, DerivedLine>);
        static_assert(std::same_as<std::ranges::range_value_t<decltype(triangles)>, DerivedTriangle>);
        EXPECT_EQ(lines.begin()->marker, 17);
        EXPECT_EQ(triangles.begin()->marker, 23);
        aho_test::expect_coordinates(lines.begin()->pos1, 1, 2, 3);
        aho_test::expect_coordinates(triangles.begin()->pos3, 7, 8, 9);
        line.pos1 = aho_test::geometry<P>(10, 11, 12);
        aho_test::expect_coordinates(lines.begin()->pos1, 1, 2, 3);
    }
    TYPED_TEST(DerivedShapeTest, CompositeViewsAcceptCollectionsOfPubliclyDerivedShapes) {
        using P = typename TypeParam::Point;
        using L = aho::Line<typename TypeParam::element_type, typename TypeParam::dimension>;
        using Triangle = aho::Triangle<typename TypeParam::element_type, typename TypeParam::dimension>;
        struct DerivedLine : L {
            using L::L;
        };
        struct DerivedTriangle : Triangle {
            using Triangle::Triangle;
        };
        struct Collection {
            using element_type = typename TypeParam::element_type;
            using dimension = typename TypeParam::dimension;
            using coordinate_info = typename P::coordinate_info;
            DerivedLine edge;
            DerivedTriangle face;
            auto lines() const { return std::array{edge}; }
            auto triangles() const { return std::array{face}; }
        };
        static_assert(aho::concepts::is_lines_object<Collection>);
        static_assert(aho::concepts::is_triangles_object<Collection>);
        const auto first = aho_test::geometry<P>(1, 2, 3);
        const auto second = aho_test::geometry<P>(4, 5, 6);
        const Collection collection{DerivedLine(first, second),
                                    DerivedTriangle(first, second, aho_test::geometry<P>(7, 8, 9))};
        const auto lines = aho::views::lines(collection);
        const auto triangles = aho::views::triangles(collection);
        ASSERT_EQ(lines.size(), 1u);
        ASSERT_EQ(triangles.size(), 1u);
        aho_test::expect_coordinates(lines[0].pos2, 4, 5, 6);
        aho_test::expect_coordinates(triangles[0].pos3, 7, 8, 9);
    }
    TEST(PolygonConceptsTest, ShapesKeepTheirCustomCoordinatePlane) {
        using CI = aho::coordinate::_MakeCoordinateInfo<false, true, true>::value;
        using P = aho::_Point<double, vsl::D2, CI>;
        using L = aho::Line<double, vsl::D2, CI>;
        using Triangle = aho::Triangle<double, vsl::D2, CI>;
        struct DerivedLine : L {
            using L::L;
        };
        struct DerivedTriangle : Triangle {
            using Triangle::Triangle;
        };
        static_assert(aho::concepts::is_line<DerivedLine> && aho::concepts::is_triangle<DerivedTriangle>);
        const auto lines = aho::views::lines(DerivedLine(P(1, 2), P(3, 4)));
        const auto triangles = aho::views::triangles(DerivedTriangle(P(1, 2), P(3, 4), P(5, 6)));
        static_assert(std::same_as<typename std::ranges::range_value_t<decltype(lines)>::coordinate_info, CI>);
        static_assert(std::same_as<typename std::ranges::range_value_t<decltype(triangles)>::coordinate_info, CI>);
        EXPECT_EQ(lines.begin()->pos2.value.y.value, 3);
        EXPECT_EQ(lines.begin()->pos2.value.z.value, 4);
        EXPECT_EQ(triangles.begin()->pos3.value.y.value, 5);
        EXPECT_EQ(triangles.begin()->pos3.value.z.value, 6);
    }
    TEST(PolygonConceptsTest, ReverseConversionsAndPrivateInheritanceDoNotIdentifyShapes) {
        using L = aho::Line<double, vsl::D2>;
        using Triangle = aho::Triangle<double, vsl::D2>;
        struct FromLine {
            using element_type = double;
            using dimension = vsl::D2;
            using coordinate_info = L::coordinate_info;
            FromLine(L) {}
        };
        struct FromTriangle {
            using element_type = double;
            using dimension = vsl::D2;
            using coordinate_info = Triangle::coordinate_info;
            FromTriangle(Triangle) {}
        };
        struct PrivateLine : private L {
            using element_type = double;
            using dimension = vsl::D2;
            using coordinate_info = L::coordinate_info;
        };
        struct PrivateTriangle : private Triangle {
            using element_type = double;
            using dimension = vsl::D2;
            using coordinate_info = Triangle::coordinate_info;
        };
        EXPECT_FALSE(aho::concepts::is_line<FromLine>);
        EXPECT_FALSE(aho::concepts::is_triangle<FromTriangle>);
        EXPECT_FALSE(aho::concepts::is_line<PrivateLine>);
        EXPECT_FALSE(aho::concepts::is_triangle<PrivateTriangle>);
    }

} // namespace
#endif

namespace {
    template<typename Dimension>
    struct DimensionMetadataShapes {
        using element_type = double;
        using dimension = Dimension;
        using P = aho::d2::PointD;
        using coordinate_info = P::coordinate_info;
        auto points() const { return std::array{P(1, 2), P(3, 4)}; }
        auto lines() const { return std::array{aho::Line<double, vsl::D2>(P(1, 2), P(3, 4))}; }
        auto triangles() const { return std::array{aho::Triangle<double, vsl::D2>(P(0, 0), P(3, 0), P(0, 4))}; }
    };
    static_assert(aho::concepts::is_points_object<DimensionMetadataShapes<vsl::D2>>);
    static_assert(aho::concepts::is_lines_object<DimensionMetadataShapes<vsl::D2>>);
    static_assert(aho::concepts::is_triangles_object<DimensionMetadataShapes<vsl::D2>>);
    static_assert(!aho::concepts::is_points_object<DimensionMetadataShapes<int>>);
    static_assert(!aho::concepts::is_lines_object<DimensionMetadataShapes<int>>);
    static_assert(!aho::concepts::is_triangles_object<DimensionMetadataShapes<int>>);
    static_assert(!aho::concepts::is_points_object<DimensionMetadataShapes<void>>);
    static_assert(!aho::concepts::is_lines_object<DimensionMetadataShapes<void>>);
    static_assert(!aho::concepts::is_triangles_object<DimensionMetadataShapes<void>>);
    TEST(PolygonDimensionRequirementTest, ValidCompositeShapesKeepTheirViewsAndCoordinates) {
        const DimensionMetadataShapes<vsl::D2> shapes;
        const auto points = aho::views::points(shapes);
        const auto lines = aho::views::lines(shapes);
        const auto triangles = aho::views::triangles(shapes);
        ASSERT_EQ(points.size(), 2u);
        ASSERT_EQ(lines.size(), 1u);
        ASSERT_EQ(triangles.size(), 1u);
        EXPECT_DOUBLE_EQ(points[1].value.x.value, 3);
        EXPECT_DOUBLE_EQ(points[1].value.y.value, 4);
        EXPECT_DOUBLE_EQ(lines[0].pos2.value.x.value, 3);
        EXPECT_DOUBLE_EQ(triangles[0].area(), 6);
    }
} // namespace
