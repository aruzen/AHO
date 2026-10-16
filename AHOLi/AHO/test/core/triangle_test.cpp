#include <AHO/core/triangle.hpp>
#if defined(AHO_CONTRACT_TRIANGLE_TRIANGLES)
void verify() { const auto result = aho::Triangle<double, vsl::D2>{}.triangles(); }
#elif defined(AHO_CONTRACT_TRIANGLE_POINTER_POINTS)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.ptrpoints(); }
#elif defined(AHO_CONTRACT_TRIANGLE_POINTER_LINES)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.ptrlines(); }
#elif defined(AHO_CONTRACT_TRIANGLE_POINTER_TRIANGLES)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.ptrtriangles(); }
#elif defined(AHO_CONTRACT_PTRTRIANGLE_LINES)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.adress().lines(); }
#elif defined(AHO_CONTRACT_PTRTRIANGLE_TRIANGLES)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.adress().triangles(); }
#elif defined(AHO_CONTRACT_PTRTRIANGLE_POINTER_POINTS)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.adress().ptrpoints(); }
#elif defined(AHO_CONTRACT_PTRTRIANGLE_POINTER_LINES)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.adress().ptrlines(); }
#elif defined(AHO_CONTRACT_TRIANGLE_POLYGON_ADDITION)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle + triangle; }
#elif defined(AHO_CONTRACT_PTRTRIANGLE_POLYGON_ADDITION)
void verify() { aho::Triangle<double, vsl::D2> triangle; const auto result = triangle.adress() + triangle; }
#else
#include "../support/geometry.hpp"
namespace {
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
    EXPECT_DOUBLE_EQ(lines[0].length(), 3); EXPECT_DOUBLE_EQ(lines[1].length(), 5); EXPECT_DOUBLE_EQ(lines[2].length(), 4);
}
TEST(TriangleTest, PointerTriangleBorrowsVerticesAndReflectsChanges) {
    aho::Triangle<double, vsl::D2> triangle(aho::d2::PointD(0, 0), aho::d2::PointD(3, 0), aho::d2::PointD(0, 4));
    const auto pointer = triangle.adress();
    aho::PtrTriangle<double, vsl::D2> assigned;
    assigned.pos1 = &triangle.pos1; assigned.pos2 = &triangle.pos2; assigned.pos3 = &triangle.pos3;
    EXPECT_DOUBLE_EQ(assigned.area(), 6);
    EXPECT_EQ(pointer.pos1, &triangle.pos1); EXPECT_EQ(pointer.pos2, &triangle.pos2); EXPECT_EQ(pointer.pos3, &triangle.pos3);
    EXPECT_DOUBLE_EQ(pointer.area(), 6);
    aho_test::expect_coordinates(pointer.points()[2], 0, 4);
    EXPECT_DOUBLE_EQ(pointer.inderect().area(), 6);
    EXPECT_EQ(pointer.ptrtriangles()[0].pos3, &triangle.pos3);
    triangle.pos3 = aho::d2::PointD(0, 8);
    EXPECT_DOUBLE_EQ(pointer.area(), 12);
}
}
#endif
