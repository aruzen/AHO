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
constexpr aho::d1::Point<int> single(-1);
constexpr aho::d2::Point<int> pair(-1, 2);
constexpr aho::d3::Point<int> triple(-1, 2, -3);
static_assert(single.value.x.value == -1);
static_assert(pair.value.x.value == -1 && pair.value.y.value == 2);
static_assert(triple.value.x.value == -1 && triple.value.y.value == 2 && triple.value.z.value == -3);
#else
#include "../support/one_way_cast.hpp"
namespace {
template<class T> class PointOneWayCastTest : public testing::Test {};
using OneWayPoints = testing::Types<aho::_Point<aho_test::CastSource, vsl::D1>,
    aho::_Point<aho_test::CastSource, vsl::D2>, aho::_Point<aho_test::CastSource, vsl::D3>,
    aho::_Point<aho_test::CastSource, vsl::D2, aho::coordinate::_MakeCoordinateInfo<false, true, true>::value>>;
TYPED_TEST_SUITE(PointOneWayCastTest, OneWayPoints);
TYPED_TEST(PointOneWayCastTest, ChecksConversionFromTheStoredScalarToTheRequestedType) {
    static_assert(aho_test::HasCast<TypeParam, aho_test::CastTarget>);
    const TypeParam source = [] {
        if constexpr (TypeParam::dimension::dimension == 1)
            return TypeParam(aho_test::CastSource{2});
        else if constexpr (TypeParam::dimension::dimension == 2)
            return TypeParam(aho_test::CastSource{2}, aho_test::CastSource{-3});
        else
            return TypeParam(aho_test::CastSource{2}, aho_test::CastSource{-3}, aho_test::CastSource{5});
    }();
    const auto converted = source.template cast<aho_test::CastTarget>();
    using Converted = std::remove_cv_t<decltype(converted)>;
    static_assert(!aho_test::HasCast<Converted, aho_test::CastSource>);
    static_assert(std::same_as<typename Converted::coordinate_info, typename TypeParam::coordinate_info>);
    auto check = [](const auto &from, const auto &to, int expected) {
        EXPECT_EQ(from.value.value, expected);
        EXPECT_EQ(to.value.value, expected);
    };
    if constexpr (TypeParam::coordinate_info::EnabledX) check(source.value.x, converted.value.x, 2);
    if constexpr (TypeParam::coordinate_info::EnabledY)
        check(source.value.y, converted.value.y, TypeParam::coordinate_info::EnabledX ? -3 : 2);
    if constexpr (TypeParam::coordinate_info::EnabledZ)
        check(source.value.z, converted.value.z, TypeParam::dimension::dimension == 3 ? 5 : -3);
}
} // namespace
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
TEST(PointDefaultCoordinatesTest, NamedDimensionsUseTheirDefaultAxes) {
    using P1 = aho::d1::Point<int>;
    using P2 = aho::d2::Point<int>;
    using P3 = aho::d3::Point<int>;
    static_assert(std::same_as<P1::coordinate_info, aho::d1::PointI::coordinate_info>);
    static_assert(std::same_as<P2::coordinate_info, aho::d2::PointI::coordinate_info>);
    static_assert(std::same_as<P3::coordinate_info, aho::d3::PointI::coordinate_info>);
    aho_test::expect_coordinates(P1{}, 0);
    aho_test::expect_coordinates(P2{}, 0, 0);
    aho_test::expect_coordinates(P3{}, 0, 0, 0);
    aho_test::expect_coordinates(P1(-1), -1);
    aho_test::expect_coordinates(P2(-1, 2), -1, 2);
    aho_test::expect_coordinates(P3(-1, 2, -3), -1, 2, -3);
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
TEST(PointLengthRangeTest, LargeIntegerCoordinatesHaveFiniteKnownLengths) {
    const aho::d2::PointI plane(60000, -80000);
    const aho::d3::PointI space(20000, -30000, 60000);
    EXPECT_DOUBLE_EQ(plane.length(), 100000);
    EXPECT_DOUBLE_EQ(space.length(), 70000);
}
template<class T> class PointFloatingLengthRangeTest : public testing::Test {};
using PointFloatingLengthScalars = testing::Types<float, double>;
TYPED_TEST_SUITE(PointFloatingLengthRangeTest, PointFloatingLengthScalars);
TYPED_TEST(PointFloatingLengthRangeTest, LargeAndSmallCoordinatesAvoidIntermediateOverflowAndUnderflow) {
    using T = TypeParam;
    // Scaled 3-4-5 and 2-3-6-7 triples need only a few rounded operations.
    constexpr T tolerance = 32 * std::numeric_limits<T>::epsilon();
    constexpr T large = static_cast<T>(std::is_same_v<T, float> ? 1e30 : 1e200);
    constexpr T small = static_cast<T>(std::is_same_v<T, float> ? 1e-30 : 1e-200);
    for (const T scale : {large, small}) {
        const aho::_Point<T, vsl::D2> plane(-T{3} * scale, T{4} * scale);
        const aho::_Point<T, vsl::D3> space(T{2} * scale, -T{3} * scale, T{6} * scale);
        EXPECT_TRUE(std::isfinite(plane.length()));
        EXPECT_TRUE(std::isfinite(space.length()));
        EXPECT_GT(plane.length(), 0);
        EXPECT_GT(space.length(), 0);
        EXPECT_NEAR(plane.length() / scale, 5, tolerance);
        EXPECT_NEAR(space.length() / scale, 7, tolerance);
    }
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
template<class Scalar> class PointOneDimensionalLengthTest : public testing::Test {};
using PointLengthScalars = testing::Types<int, float, double>;
TYPED_TEST_SUITE(PointOneDimensionalLengthTest, PointLengthScalars);
TYPED_TEST(PointOneDimensionalLengthTest, EachAxisHasNonnegativeLengthForEitherCoordinateSign) {
    using T = TypeParam;
    using X = aho::coordinate::_MakeCoordinateInfo<true, false, false>::value;
    using Y = aho::coordinate::_MakeCoordinateInfo<false, true, false>::value;
    using Z = aho::coordinate::_MakeCoordinateInfo<false, false, true>::value;
    []<class... Axes>(std::tuple<Axes...>) {
        ([]<class Axis>() {
            using P = aho::_Point<T, vsl::D1, Axis>;
            EXPECT_EQ(P(T{-3}).length(), 3);
            EXPECT_EQ(P(T{3}).length(), 3);
            EXPECT_EQ(P(T{}).length(), 0);
        }.template operator()<Axes>(), ...);
    }(std::tuple<X, Y, Z>{});
}
TEST(PointOneDimensionalLengthTest, MinimumSignedCoordinateHasRepresentablePositiveLength) {
    constexpr aho::d1::PointI negative(-3);
    static_assert(negative.length() == 3);
    static_assert(std::same_as<decltype(negative.length()), unsigned>);
    static_assert(std::same_as<decltype(aho::d1::PointF(-3).length()), float>);
    static_assert(std::same_as<decltype(aho::d1::PointD(-3).length()), double>);
    const aho::d1::PointI minimum(std::numeric_limits<int>::min());
    EXPECT_EQ(minimum.length(), -static_cast<double>(std::numeric_limits<int>::min()));
    EXPECT_EQ(minimum.value.x.value, std::numeric_limits<int>::min());
}
TEST(PointOneDimensionalLengthTest, LargeIntegerMagnitudesKeepTheirExactValues) {
    const aho::_Point<long long, vsl::D1> large(9007199254740993LL);
    static_assert(std::same_as<decltype(large.length()), unsigned long long>);
    EXPECT_EQ(large.length(), 9007199254740993ULL);
    const aho::_Point<unsigned long long, vsl::D1> maximum(std::numeric_limits<unsigned long long>::max());
    EXPECT_EQ(maximum.length(), std::numeric_limits<unsigned long long>::max());
}
TEST(PointOneDimensionalLengthTest, UnsignedCoordinatesRemainPositiveAndNegativeZeroBecomesPositiveZero) {
    const aho::_Point<unsigned, vsl::D1> maximum(std::numeric_limits<unsigned>::max());
    EXPECT_EQ(maximum.length(), static_cast<double>(std::numeric_limits<unsigned>::max()));
    const aho::d1::PointD zero(-0.0);
    EXPECT_EQ(zero.length(), 0);
    EXPECT_FALSE(std::signbit(zero.length()));
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
