#include <gtest/gtest.h>
#include <AHO/core/point.hpp>
#if defined(AHO_CONTRACT_POINT_CAST_3D)
#include <gtest/gtest.h>
TEST(PointContractTest, ThreeDimensionalCastPreservesAllCoordinates) {
    const auto result = aho::d3::PointD(1, 2, 3).cast<float>();
    EXPECT_FLOAT_EQ(result.value.x.value, 1); EXPECT_FLOAT_EQ(result.value.y.value, 2); EXPECT_FLOAT_EQ(result.value.z.value, 3);
}
#elif defined(AHO_CONTRACT_POINT_COMPARISON)
TEST(PointContractTest, ThreeWayComparisonOrdersAllDimensionsAndRecognizesEquality) {
    const auto d1 = aho::d1::PointD(1) <=> aho::d1::PointD(2);
    const auto d2 = aho::d2::PointD(1, 2) <=> aho::d2::PointD(2, 3);
    const auto d3 = aho::d3::PointD(1, 2, 3) <=> aho::d3::PointD(2, 3, 4);
    EXPECT_TRUE(std::is_lt(d1)); EXPECT_TRUE(std::is_lt(d2)); EXPECT_TRUE(std::is_lt(d3));
    EXPECT_TRUE(aho::d1::PointD(1) == aho::d1::PointD(1));
    EXPECT_TRUE(aho::d2::PointD(1, 2) == aho::d2::PointD(1, 2));
    EXPECT_TRUE(aho::d3::PointD(1, 2, 3) == aho::d3::PointD(1, 2, 3));
}
#elif defined(AHO_CONTRACT_POINT_CONSTEXPR_ARITHMETIC)
constexpr auto sum = aho::d2::PointI(1, 2) + aho::d2::VectorI(3, 4);
static_assert(sum.value.x.value == 4 && sum.value.y.value == 6);
#elif defined(AHO_CONTRACT_POINT_DEFAULT_COORDINATES)
void verify() {
    const aho::d1::Point<int> single(1);
    const aho::d2::Point<int> pair(1, 2);
    const aho::d3::Point<int> triple(1, 2, 3);
}
#else
#include "../support/geometry.hpp"

namespace {
template<class T> class PointTest : public testing::Test {};
TYPED_TEST_SUITE(PointTest, aho_test::GeometryCases);
TYPED_TEST(PointTest, DefaultScalarAndCoordinateConstructorsPreserveValues) {
    using P = typename TypeParam::Point;
    aho_test::expect_coordinates(P{}, 0, 0, 0);
    const auto value = aho_test::geometry<P>(2, 3, 4);
    aho_test::expect_coordinates(value, 2, 3, 4);
    aho_test::expect_coordinates(P(value.value), 2, 3, 4);
}
TYPED_TEST(PointTest, UnaryOperatorsPreserveCoordinates) {
    using P = typename TypeParam::Point;
    const auto value = aho_test::geometry<P>(2, 3, 4);
    aho_test::expect_coordinates(+value, 2, 3, 4);
    aho_test::expect_coordinates(-value, -2, -3, -4);
}
TEST(PointCastTest, OneAndTwoDimensionalCastsPreserveCoordinates) {
    aho_test::expect_coordinates(aho::d1::PointI(2).cast<double>(), 2);
    aho_test::expect_coordinates(aho::d2::PointI(2, 3).cast<double>(), 2, 3);
    aho_test::expect_coordinates(aho::d1::PointD(-2.75).cast<int>(), -2);
    aho_test::expect_coordinates(aho::d2::PointD(-2.75, 3.75).cast<int>(), -2, 3);
}
TEST(PointCastTest, ThreeDimensionalCastPreservesAllSignedCoordinatesAndSource) {
    const aho::d3::PointI source(-2, 3, -7);
    const auto floating = source.cast<double>();
    static_assert(std::same_as<std::remove_cv_t<decltype(floating)>, aho::d3::PointD>);
    aho_test::expect_coordinates(floating, -2, 3, -7);
    aho_test::expect_coordinates(floating.cast<float>(), -2, 3, -7);
    aho_test::expect_coordinates(source, -2, 3, -7);
}
TEST(PointCastTest, ThreeDimensionalIntegralCastTruncatesEveryComponentTowardZero) {
    const aho::d3::PointD source(-2.75, 3.75, -7.5);
    aho_test::expect_coordinates(source.cast<int>(), -2, 3, -7);
    aho_test::expect_coordinates(source, -2.75, 3.75, -7.5);
}
TEST(PointDeductionTest, DeductionAndExplicitCoordinateWrappersPreserveValues) {
    const aho::Point default_value;
    aho_test::expect_coordinates(default_value, 0);
    const aho::Point single(2);
    const aho::Point pair(2, 3);
    const aho::Point triple(2, 3, 4);
    aho_test::expect_coordinates(single, 2);
    aho_test::expect_coordinates(pair, 2, 3);
    aho_test::expect_coordinates(triple, 2, 3, 4);
    using CI1 = aho::coordinate::_DefaultCoordinateInfo<vsl::D1>::value;
    using CI2 = aho::coordinate::_DefaultCoordinateInfo<vsl::D2>::value;
    using CI3 = aho::coordinate::_DefaultCoordinateInfo<vsl::D3>::value;
    aho_test::expect_coordinates(aho::d1::Point<int, CI1>(2), 2);
    aho_test::expect_coordinates(aho::d2::Point<int, CI2>(2, 3), 2, 3);
    aho_test::expect_coordinates(aho::d3::Point<int, CI3>(2, 3, 4), 2, 3, 4);
    aho_test::expect_coordinates(aho::d1::Point<int, CI1>{}, 0);
    aho_test::expect_coordinates(aho::d2::Point<int, CI2>{}, 0, 0);
    aho_test::expect_coordinates(aho::d3::Point<int, CI3>{}, 0, 0, 0);
}
TYPED_TEST(PointTest, PointDifferenceProducesVectorAndTranslationProducesPoint) {
    using P = typename TypeParam::Point;
    using V = typename TypeParam::Vector;
    const auto a = aho_test::geometry<P>(2, 3, 4), b = aho_test::geometry<P>(1, 1, 1);
    const auto delta = aho_test::geometry<V>(1, 2, 3);
    static_assert(std::same_as<decltype(a - b), V>);
    aho_test::expect_coordinates(a - b, 1, 2, 3);
    aho_test::expect_coordinates(a + delta, 3, 5, 7);
    aho_test::expect_coordinates(a - delta, 1, 1, 1);
}
TYPED_TEST(PointTest, ScalarOperatorsAndCompoundOperatorsMutateCoordinates) {
    using P = typename TypeParam::Point;
    using V = typename TypeParam::Vector;
    using T = typename TypeParam::element_type;
    auto value = aho_test::geometry<P>(2, 4, 6);
    aho_test::expect_coordinates(value * T{2}, 4, 8, 12);
    aho_test::expect_coordinates(value / T{2}, 1, 2, 3);
    const auto delta = aho_test::geometry<V>(1, 2, 3);
    EXPECT_EQ(&(value += delta), &value); aho_test::expect_coordinates(value, 3, 6, 9);
    EXPECT_EQ(&(value -= delta), &value); aho_test::expect_coordinates(value, 2, 4, 6);
    EXPECT_EQ(&(value *= T{2}), &value); aho_test::expect_coordinates(value, 4, 8, 12);
    EXPECT_EQ(&(value /= T{2}), &value); aho_test::expect_coordinates(value, 2, 4, 6);
}
TYPED_TEST(PointTest, LengthAndOrderingHandleKnownCoordinates) {
    using P = typename TypeParam::Point;
    const auto value = aho_test::geometry<P>(3, 4, 12);
    const double expected = TypeParam::dimension::dimension == 1 ? 3 : TypeParam::dimension::dimension == 2 ? 5 : 13;
    EXPECT_DOUBLE_EQ(value.length(), expected);
    EXPECT_TRUE(aho_test::geometry<P>(1) < value);
}
}
#endif
