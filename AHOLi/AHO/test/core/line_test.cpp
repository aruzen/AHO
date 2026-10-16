#include <AHO/core/triangle.hpp>
#if defined(AHO_CONTRACT_LINE_POINTER_POINTS)
void verify() { aho::Line<double, vsl::D2> line; const auto points = line.ptrpoints(); }
#elif defined(AHO_CONTRACT_PTRLINE_POINTER_POINTS)
void verify() { aho::d2::PointD a(1, 2), b(3, 4); const auto points = aho::PtrLine<double, vsl::D2>(&a, &b).ptrpoints(); }
#elif defined(AHO_CONTRACT_LINE_POLYGON_ADDITION)
void verify() { aho::Line<double, vsl::D2> line; const auto polygon = line + line; }
#elif defined(AHO_CONTRACT_PTRLINE_POLYGON_ADDITION)
void verify() { aho::Line<double, vsl::D2> line; const auto polygon = line.adress() + line; }
#else
#include "../support/geometry.hpp"
namespace {
TEST(LineTest, ConstructorsLengthAndValueViewsPreserveEndpoints) {
    using P = aho::d2::PointD;
    aho::Line<double, vsl::D2> line(P(0, 0), P(3, 4));
    EXPECT_DOUBLE_EQ(line.length(), 5);
    aho_test::expect_coordinates(line.points()[0], 0, 0);
    aho_test::expect_coordinates(line.points()[1], 3, 4);
    EXPECT_DOUBLE_EQ(line.lines()[0].length(), 5);
    const auto converted = line.cast<float>();
    aho_test::expect_coordinates(converted.pos1, 0, 0);
    aho_test::expect_coordinates(converted.pos2, 3, 4);
    aho::Line<double, vsl::D2> zero;
    EXPECT_DOUBLE_EQ(zero.length(), 0);
}
TEST(LineTest, AddressAndPointerLineViewsReferToOriginalEndpoints) {
    aho::Line<double, vsl::D2> line(aho::d2::PointD(0, 0), aho::d2::PointD(3, 4));
    const auto pointer = line.adress();
    aho::PtrLine<double, vsl::D2> assigned;
    assigned.pos1 = &line.pos1; assigned.pos2 = &line.pos2;
    EXPECT_DOUBLE_EQ(assigned.length(), 5);
    EXPECT_EQ(pointer.pos1, &line.pos1); EXPECT_EQ(pointer.pos2, &line.pos2);
    EXPECT_EQ(line.ptrlines()[0].pos2, &line.pos2);
    EXPECT_EQ(pointer.ptrlines()[0].pos1, &line.pos1);
    EXPECT_DOUBLE_EQ(pointer.length(), 5);
    aho_test::expect_coordinates(pointer.points()[1], 3, 4);
    EXPECT_DOUBLE_EQ(pointer.lines()[0].length(), 5);
    EXPECT_DOUBLE_EQ(pointer.indirect().length(), 5);
    line.pos2 = aho::d2::PointD(6, 8);
    EXPECT_DOUBLE_EQ(pointer.length(), 10);
}
TEST(LineTest, AddingPointCreatesTriangleInEndpointOrder) {
    aho::Line<double, vsl::D2> line(aho::d2::PointD(0, 0), aho::d2::PointD(3, 0));
    const auto triangle = line + aho::d2::PointD(0, 4);
    EXPECT_DOUBLE_EQ(triangle.area(), 6);
    const auto pointer_triangle = line.adress() + aho::d2::PointD(0, 4);
    EXPECT_DOUBLE_EQ(pointer_triangle.area(), 6);
}
}
#endif
