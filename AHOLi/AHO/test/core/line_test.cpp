#include <AHO/core/triangle.hpp>
#if defined(AHO_CONTRACT_LINE_POINTER_POINTS)
void verify() {
    aho::Line<double, vsl::D2> line;
    const auto points = line.ptrpoints();
}
#elif defined(AHO_CONTRACT_PTRLINE_POINTER_POINTS)
void verify() {
    aho::d2::PointD a(1, 2), b(3, 4);
    const auto points = aho::PtrLine<double, vsl::D2>(&a, &b).ptrpoints();
}
#elif defined(AHO_CONTRACT_LINE_POLYGON_ADDITION)
void verify() {
    aho::Line<double, vsl::D2> line;
    const auto polygon = line + line;
}
#elif defined(AHO_CONTRACT_PTRLINE_POLYGON_ADDITION)
void verify() {
    aho::Line<double, vsl::D2> line;
    const auto polygon = line.adress() + line;
}
#else
#include <gtest/gtest.h>
#include "../support/one_way_cast.hpp"
namespace {
    TEST(LineOneWayCastTest, ConvertsBothEndpointsAndRejectsTheReverseConversion) {
        using P = aho::_Point<aho_test::CastSource, vsl::D2>;
        using L = aho::Line<aho_test::CastSource, vsl::D2>;
        static_assert(aho_test::HasCast<L, aho_test::CastTarget>);
        L source(P(aho_test::CastSource{1}, aho_test::CastSource{2}),
                 P(aho_test::CastSource{3}, aho_test::CastSource{4}));
        const auto converted = source.cast<aho_test::CastTarget>();
        using Converted = std::remove_cv_t<decltype(converted)>;
        static_assert(!aho_test::HasCast<Converted, aho_test::CastSource>);
        EXPECT_EQ(converted.pos1.value.x.value.value, 1);
        EXPECT_EQ(converted.pos1.value.y.value.value, 2);
        EXPECT_EQ(converted.pos2.value.x.value.value, 3);
        EXPECT_EQ(converted.pos2.value.y.value.value, 4);
        EXPECT_EQ(source.pos1.value.x.value.value, 1);
        EXPECT_EQ(source.pos2.value.y.value.value, 4);
    }
} // namespace
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
        assigned.pos1 = &line.pos1;
        assigned.pos2 = &line.pos2;
        EXPECT_DOUBLE_EQ(assigned.length(), 5);
        EXPECT_EQ(pointer.pos1, &line.pos1);
        EXPECT_EQ(pointer.pos2, &line.pos2);
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
    template<class T>
    class LineConstCastTest : public testing::Test {};
    TYPED_TEST_SUITE(LineConstCastTest, aho_test::GeometryCases);
    TYPED_TEST(LineConstCastTest, ConstLinesConvertBothEndpointsAndKeepIndependentValues) {
        using P = typename TypeParam::Point;
        using L = aho::Line<typename TypeParam::element_type, typename TypeParam::dimension>;
        using Converted = aho::Line<int, typename TypeParam::dimension>;
        const L source(aho_test::geometry<P>(1.5, -2.5, 3.5), aho_test::geometry<P>(4.75, -5.75, 6.75));
        const auto original = source;
        auto converted = source.template cast<int>();
        static_assert(std::same_as<decltype(converted), Converted>);
        aho_test::expect_coordinates(converted.pos1, 1, -2, 3);
        aho_test::expect_coordinates(converted.pos2, 4, -5, 6);
        converted.pos1 = aho_test::geometry<decltype(converted.pos1)>(10, 11, 12);
        EXPECT_EQ(source.pos1, original.pos1);
        EXPECT_EQ(source.pos2, original.pos2);
    }
    TEST(LineConstCastTest, ConstLinesKeepTheirCustomCoordinatePlane) {
        using CI = aho::coordinate::_MakeCoordinateInfo<false, true, true>::value;
        using P = aho::_Point<double, vsl::D2, CI>;
        const aho::Line<double, vsl::D2, CI> source(P(1.5, 2.75), P(3.5, 4.25));
        const auto converted = source.cast<int>();
        static_assert(std::same_as<std::remove_cv_t<decltype(converted)>, aho::Line<int, vsl::D2, CI>>);
        EXPECT_EQ(converted.pos1.value.y.value, 1);
        EXPECT_EQ(converted.pos1.value.z.value, 2);
        EXPECT_EQ(converted.pos2.value.y.value, 3);
        EXPECT_EQ(converted.pos2.value.z.value, 4);
        EXPECT_EQ(source.pos1.value.y.value, 1.5);
        EXPECT_EQ(source.pos2.value.z.value, 4.25);
    }

    template<class T>
    class LinePointViewTest : public testing::Test {};
    TYPED_TEST_SUITE(LinePointViewTest, aho_test::GeometryCases);
    TYPED_TEST(LinePointViewTest, OwningAndBorrowedViewsExposeOriginalEndpointsInOrder) {
        using P = typename TypeParam::Point;
        using L = aho::Line<typename TypeParam::element_type, typename TypeParam::dimension>;
        L line(aho_test::geometry<P>(1, 2, 3), aho_test::geometry<P>(4, 5, 6));
        const auto direct = line.ptrpoints();
        const auto borrowed_line = line.adress();
        const auto borrowed = borrowed_line.ptrpoints();
        EXPECT_EQ(direct[0], &line.pos1);
        EXPECT_EQ(direct[1], &line.pos2);
        EXPECT_EQ(borrowed[0], &line.pos1);
        EXPECT_EQ(borrowed[1], &line.pos2);
        *direct[0] = aho_test::geometry<P>(7, 8, 9);
        aho_test::expect_coordinates(line.pos1, 7, 8, 9);
        aho_test::expect_coordinates(*borrowed[0], 7, 8, 9);
        *borrowed[1] = aho_test::geometry<P>(-1, -2, -3);
        aho_test::expect_coordinates(line.pos2, -1, -2, -3);
        aho_test::expect_coordinates(*direct[1], -1, -2, -3);
    }

}
#endif
