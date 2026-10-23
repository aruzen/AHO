#include <AHO/core/triangle.hpp>
#if defined(AHO_CONTRACT_TRIANGLE_TRIANGLES)
void verify() {
    const auto result = aho::Triangle<double, vsl::D2>{}.triangles();
}
#elif defined(AHO_CONTRACT_TRIANGLE_POINTER_POINTS)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.ptrpoints();
}
#elif defined(AHO_CONTRACT_TRIANGLE_POINTER_LINES)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.ptrlines();
}
#elif defined(AHO_CONTRACT_TRIANGLE_POINTER_TRIANGLES)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.ptrtriangles();
}
#elif defined(AHO_CONTRACT_PTRTRIANGLE_LINES)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.adress().lines();
}
#elif defined(AHO_CONTRACT_PTRTRIANGLE_TRIANGLES)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.adress().triangles();
}
#elif defined(AHO_CONTRACT_PTRTRIANGLE_POINTER_POINTS)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.adress().ptrpoints();
}
#elif defined(AHO_CONTRACT_PTRTRIANGLE_POINTER_LINES)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.adress().ptrlines();
}
#elif defined(AHO_CONTRACT_TRIANGLE_POLYGON_ADDITION)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle + triangle;
}
#elif defined(AHO_CONTRACT_PTRTRIANGLE_POLYGON_ADDITION)
void verify() {
    aho::Triangle<double, vsl::D2> triangle;
    const auto result = triangle.adress() + triangle;
}
#else
#include "../support/geometry.hpp"
namespace {
    template<class T>
    class PointerTriangleMetadataTest : public testing::Test {};
    TYPED_TEST_SUITE(PointerTriangleMetadataTest, aho_test::GeometryCases);
    TYPED_TEST(PointerTriangleMetadataTest, CoordinateMetadataEnablesPointViewsWithoutLosingVertexValues) {
        using P = typename TypeParam::Point;
        using CI = typename P::CoordinateInfo;
        using Triangle = aho::Triangle<typename TypeParam::ElementType, typename TypeParam::Dimension, CI>;
        using Borrowed = aho::PtrTriangle<typename TypeParam::ElementType, typename TypeParam::Dimension, CI>;
        static_assert(std::same_as<typename Borrowed::CoordinateInfo, CI>);
        static_assert(aho::concepts::is_points_object<Borrowed>);
        Triangle owner(aho_test::geometry<P>(1, 2, 3), aho_test::geometry<P>(4, 5, 6), aho_test::geometry<P>(7, 8, 9));
        const auto borrowed = owner.adress();
        const auto points = aho::views::points(borrowed);
        ASSERT_EQ(points.size(), 3u);
        aho_test::expect_coordinates(points[0], 1, 2, 3);
        aho_test::expect_coordinates(points[1], 4, 5, 6);
        aho_test::expect_coordinates(points[2], 7, 8, 9);
        owner.pos3 = aho_test::geometry<P>(-1, -2, -3);
        aho_test::expect_coordinates(points[2], 7, 8, 9);
        aho_test::expect_coordinates(aho::views::points(borrowed)[2], -1, -2, -3);
    }
    TEST(TriangleTest, TwoAndThreeDimensionalAreaHandlesWindingAndDegenerateTriangles) {
        const aho::Triangle<double, vsl::D2> a(aho::d2::PointD(0, 0), aho::d2::PointD(3, 0), aho::d2::PointD(0, 4));
        EXPECT_DOUBLE_EQ(a.area(), 6);
        EXPECT_DOUBLE_EQ((aho::Triangle<double, vsl::D2>(a.pos3, a.pos2, a.pos1).area()), 6);
        EXPECT_DOUBLE_EQ((aho::Triangle<double, vsl::D2>(a.pos1, a.pos1, a.pos3).area()), 0);
        const aho::Triangle<double, vsl::D3> b(aho::d3::PointD(0, 0, 0), aho::d3::PointD(3, 0, 0), aho::d3::PointD(0, 0, 4));
        EXPECT_DOUBLE_EQ(b.area(), 6);
        EXPECT_DOUBLE_EQ((aho::Triangle<double, vsl::D2>{}.area()), 0);
    }
    TEST(TriangleTest, PointAndLineViewsFollowVertexOrderAndCloseTheOutline) {
        const aho::Triangle<double, vsl::D2> triangle(aho::d2::PointD(0, 0), aho::d2::PointD(3, 0), aho::d2::PointD(0, 4));
        const auto points = triangle.points();
        aho_test::expect_coordinates(points[0], 0, 0);
        aho_test::expect_coordinates(points[1], 3, 0);
        aho_test::expect_coordinates(points[2], 0, 4);
        const auto lines = triangle.lines();
        EXPECT_DOUBLE_EQ(lines[0].length(), 3);
        EXPECT_DOUBLE_EQ(lines[1].length(), 5);
        EXPECT_DOUBLE_EQ(lines[2].length(), 4);
    }
    TEST(TriangleTest, PointerTriangleBorrowsVerticesAndReflectsChanges) {
        aho::Triangle<double, vsl::D2> triangle(aho::d2::PointD(0, 0), aho::d2::PointD(3, 0), aho::d2::PointD(0, 4));
        const auto pointer = triangle.adress();
        aho::PtrTriangle<double, vsl::D2> assigned;
        assigned.pos1 = &triangle.pos1;
        assigned.pos2 = &triangle.pos2;
        assigned.pos3 = &triangle.pos3;
        EXPECT_DOUBLE_EQ(assigned.area(), 6);
        EXPECT_EQ(pointer.pos1, &triangle.pos1);
        EXPECT_EQ(pointer.pos2, &triangle.pos2);
        EXPECT_EQ(pointer.pos3, &triangle.pos3);
        EXPECT_DOUBLE_EQ(pointer.area(), 6);
        aho_test::expect_coordinates(pointer.points()[2], 0, 4);
        EXPECT_DOUBLE_EQ(pointer.inderect().area(), 6);
        EXPECT_EQ(pointer.ptrtriangles()[0].pos3, &triangle.pos3);
        triangle.pos3 = aho::d2::PointD(0, 8);
        EXPECT_DOUBLE_EQ(pointer.area(), 12);
    }
    template<class T>
    class TriangleArrayViewTest : public testing::Test {};
    TYPED_TEST_SUITE(TriangleArrayViewTest, aho_test::GeometryCases);
    TYPED_TEST(TriangleArrayViewTest, PointerViewsPreserveVertexOrderAndCloseTheOutline) {
        using P = typename TypeParam::Point;
        using Triangle = aho::Triangle<typename TypeParam::ElementType, typename TypeParam::Dimension>;
        Triangle triangle(aho_test::geometry<P>(1, 2, 3), aho_test::geometry<P>(4, 5, 6), aho_test::geometry<P>(7, 8, 9));
        const auto pointer = triangle.adress();
        const auto direct_points = triangle.ptrpoints();
        const auto borrowed_points = pointer.ptrpoints();
        const auto direct_edges = triangle.ptrlines();
        const auto borrowed_edges = pointer.ptrlines();
        const auto direct_triangles = triangle.ptrtriangles();
        const auto borrowed_triangles = pointer.ptrtriangles();
        const std::array<P*, 3> vertices{&triangle.pos1, &triangle.pos2, &triangle.pos3};
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            EXPECT_EQ(direct_points[i], vertices[i]);
            EXPECT_EQ(borrowed_points[i], vertices[i]);
            EXPECT_EQ(direct_edges[i].pos1, vertices[i]);
            EXPECT_EQ(borrowed_edges[i].pos1, vertices[i]);
            EXPECT_EQ(direct_edges[i].pos2, vertices[(i + 1) % 3]);
            EXPECT_EQ(borrowed_edges[i].pos2, vertices[(i + 1) % 3]);
        }
        EXPECT_EQ(direct_triangles[0].pos1, vertices[0]);
        EXPECT_EQ(borrowed_triangles[0].pos1, vertices[0]);
        EXPECT_EQ(direct_triangles[0].pos2, vertices[1]);
        EXPECT_EQ(borrowed_triangles[0].pos2, vertices[1]);
        EXPECT_EQ(direct_triangles[0].pos3, vertices[2]);
        EXPECT_EQ(borrowed_triangles[0].pos3, vertices[2]);
        *borrowed_edges[1].pos2 = aho_test::geometry<P>(-1, -2, -3);
        aho_test::expect_coordinates(triangle.pos3, -1, -2, -3);
        aho_test::expect_coordinates(*direct_points[2], -1, -2, -3);
        aho_test::expect_coordinates(*direct_triangles[0].pos3, -1, -2, -3);
    }
    TYPED_TEST(TriangleArrayViewTest, ValueViewsCopyVerticesAndKeepTheirSnapshotAfterSourceChanges) {
        using P = typename TypeParam::Point;
        using Triangle = aho::Triangle<typename TypeParam::ElementType, typename TypeParam::Dimension>;
        Triangle triangle(aho_test::geometry<P>(1, 2, 3), aho_test::geometry<P>(4, 5, 6), aho_test::geometry<P>(7, 8, 9));
        const auto pointer = triangle.adress();
        const auto direct = triangle.triangles();
        const auto borrowed = pointer.triangles();
        const auto edges = pointer.lines();
        for (const auto* snapshot : {&direct[0], &borrowed[0]}) {
            aho_test::expect_coordinates(snapshot->pos1, 1, 2, 3);
            aho_test::expect_coordinates(snapshot->pos2, 4, 5, 6);
            aho_test::expect_coordinates(snapshot->pos3, 7, 8, 9);
        }
        aho_test::expect_coordinates(edges[0].pos1, 1, 2, 3);
        aho_test::expect_coordinates(edges[0].pos2, 4, 5, 6);
        aho_test::expect_coordinates(edges[1].pos1, 4, 5, 6);
        aho_test::expect_coordinates(edges[1].pos2, 7, 8, 9);
        aho_test::expect_coordinates(edges[2].pos1, 7, 8, 9);
        aho_test::expect_coordinates(edges[2].pos2, 1, 2, 3);
        triangle.pos1 = aho_test::geometry<P>(-1, -2, -3);
        aho_test::expect_coordinates(direct[0].pos1, 1, 2, 3);
        aho_test::expect_coordinates(borrowed[0].pos1, 1, 2, 3);
        aho_test::expect_coordinates(edges[0].pos1, 1, 2, 3);
        aho_test::expect_coordinates(pointer.triangles()[0].pos1, -1, -2, -3);
    }

} // namespace

namespace {
    template<class R, bool x, bool y, bool z>
    struct OneDimensionalTriangleCase {
        using CoordinateInfo = typename aho::coordinate::MakeCoordinateInfo<x, y, z>::Value;
        using Triangle = aho::Triangle<R, vsl::D1, CoordinateInfo>;
    };
    template<class T>
    class OneDimensionalTriangleAreaTest : public testing::Test {};
    using OneDimensionalTriangles = testing::Types<
        OneDimensionalTriangleCase<int, true, false, false>,
        OneDimensionalTriangleCase<int, false, true, false>,
        OneDimensionalTriangleCase<int, false, false, true>,
        OneDimensionalTriangleCase<double, true, false, false>,
        OneDimensionalTriangleCase<double, false, true, false>,
        OneDimensionalTriangleCase<double, false, false, true>>;
    TYPED_TEST_SUITE(OneDimensionalTriangleAreaTest, OneDimensionalTriangles);
    TYPED_TEST(OneDimensionalTriangleAreaTest, ValueAndPointerTrianglesHaveZeroAreaAtCompileTimeAndRuntime) {
        using Triangle = typename TypeParam::Triangle;
        constexpr auto has_zero_area = [] {
            Triangle triangle({-3}, {2}, {7});
            return triangle.area() == 0 && triangle.adress().area() == 0;
        };
        static_assert(has_zero_area());
        EXPECT_TRUE(has_zero_area());
        Triangle triangle({-3}, {2}, {7});
        auto borrowed = triangle.adress();
        EXPECT_DOUBLE_EQ(triangle.area(), 0);
        EXPECT_DOUBLE_EQ(borrowed.area(), 0);
        triangle.pos2 = decltype(triangle.pos2)(11);
        EXPECT_DOUBLE_EQ(borrowed.area(), 0);
    }
} // namespace

#include <cmath>
#include <cstdint>
#include <limits>
namespace {
    template<class T, bool x, bool y, bool z>
    struct TriangleAreaRangeCase {
        using Scalar = T;
        using CI = typename aho::coordinate::MakeCoordinateInfo<x, y, z>::Value;
        using Point = aho::Point_<T, typename CI::Dimension, CI>;
        using Triangle = aho::Triangle<T, typename CI::Dimension, CI>;
        static Point Point_(T first, T second) {
            if constexpr (CI::Dimension::DIMENSION == 2)
                return Point(first, second);
            else
                return Point(first, second, T{});
        }
    };
    template<class T>
    class TriangleAreaRangeTest : public testing::Test {};
    using TriangleAreaRanges = testing::Types<
        TriangleAreaRangeCase<float, true, true, false>,
        TriangleAreaRangeCase<float, true, false, true>,
        TriangleAreaRangeCase<float, false, true, true>,
        TriangleAreaRangeCase<float, true, true, true>,
        TriangleAreaRangeCase<double, true, true, false>,
        TriangleAreaRangeCase<double, true, false, true>,
        TriangleAreaRangeCase<double, false, true, true>,
        TriangleAreaRangeCase<double, true, true, true>>;
    TYPED_TEST_SUITE(TriangleAreaRangeTest, TriangleAreaRanges);
    TYPED_TEST(TriangleAreaRangeTest, ValueAndPointerAreasUseAWiderRangeForFloatingIntermediates) {
        using C = TypeParam;
        using T = typename C::Scalar;
        auto check = [](const char* name, typename C::Point first, typename C::Point second,
                        typename C::Point third, double expected) {
            SCOPED_TRACE(name);
            typename C::Triangle triangle(first, second, third);
            EXPECT_DOUBLE_EQ(triangle.area(), expected);
            EXPECT_DOUBLE_EQ(triangle.adress().area(), expected);
        };
        const auto zero = C::Point_(0, 0);
        const T large = std::ldexp(T{1}, 80);
        const T tiny = std::ldexp(T{1}, -80);
        check("cross product overflow", zero, C::Point_(large, 0), C::Point_(0, large), std::ldexp(1.0, 159));
        check("cross product underflow", zero, C::Point_(tiny, 0), C::Point_(0, tiny), std::ldexp(1.0, -161));
        const T endpoint = std::ldexp(T{1}, 127);
        check("subtraction overflow", C::Point_(-endpoint, 0), C::Point_(endpoint, 0), C::Point_(-endpoint, 1),
              std::ldexp(1.0, 127));
        check("mixed exponents", zero, C::Point_(large, 0), C::Point_(0, tiny), 0.5);
        const T subnormal = std::ldexp(T{1}, -149);
        check("area below float minimum", zero, C::Point_(subnormal, 0), C::Point_(0, 1), std::ldexp(1.0, -150));
    }
    TEST(TriangleAreaRangeTest, LargeUnsignedCoordinatesRetainTheUnitDeterminant) {
        using T = std::uint64_t;
        using P = aho::d2::Point<T>;
        const auto maximum = std::numeric_limits<T>::max();
        aho::Triangle<T, vsl::D2> triangle(P(0, 0), P(maximum - 1, maximum - 2), P(maximum, maximum - 1));
        EXPECT_DOUBLE_EQ(triangle.area(), 0.5);
        EXPECT_DOUBLE_EQ(triangle.adress().area(), 0.5);
    }
} // namespace
#endif
