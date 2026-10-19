#include <AHO/core/triangle.hpp>
#if defined(AHO_CONTRACT_TRIANGLE_TRIANGLES)
void verify() { const auto result = aho::Triangle<double, vsl::D2>{}.triangles(); }
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
        using CI = typename P::coordinate_info;
        using Triangle = aho::Triangle<typename TypeParam::element_type, typename TypeParam::dimension, CI>;
        using Borrowed = aho::PtrTriangle<typename TypeParam::element_type, typename TypeParam::dimension, CI>;
        static_assert(std::same_as<typename Borrowed::coordinate_info, CI>);
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
        using Triangle = aho::Triangle<typename TypeParam::element_type, typename TypeParam::dimension>;
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
        using Triangle = aho::Triangle<typename TypeParam::element_type, typename TypeParam::dimension>;
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

}
#endif
