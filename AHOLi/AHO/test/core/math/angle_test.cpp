#include <gtest/gtest.h>
#include <AHO/core/math/angle.hpp>
#include <numbers>

namespace {
template<class T> class AngleTest : public testing::Test {};
using AngleTypes = testing::Types<aho::Degree, aho::Radian>;
TYPED_TEST_SUITE(AngleTest, AngleTypes);
TYPED_TEST(AngleTest, UnaryBinaryAndComparisonOperatorsPreserveUnits) {
    const TypeParam a{12}, b{3};
    EXPECT_EQ((+a).value, 12);
    EXPECT_EQ((-a).value, -12);
    EXPECT_EQ((a + b).value, 15);
    EXPECT_EQ((a - b).value, 9);
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(a == a);
}
TYPED_TEST(AngleTest, ScalarOperatorsSupportBothOperandOrdersAndTypes) {
    const TypeParam value{12};
    auto verify = [&](auto scalar) {
        EXPECT_EQ((value * scalar).value, 24);
        EXPECT_EQ((scalar * value).value, 24);
        EXPECT_EQ((value / scalar).value, 6);
        EXPECT_NEAR(static_cast<double>((scalar / value).value), 1.0 / 6.0, 1e-15);
    };
    verify(2.0); verify(2.0f); verify(std::size_t{2});
}
TYPED_TEST(AngleTest, AssignmentOperatorsMutateAndReturnOriginalObject) {
    TypeParam value{12};
    EXPECT_EQ(&(value += TypeParam{3}), &value); EXPECT_EQ(value.value, 15);
    EXPECT_EQ(&(value -= TypeParam{3}), &value); EXPECT_EQ(value.value, 12);
    EXPECT_EQ(&(value *= 2.0), &value); EXPECT_EQ(value.value, 24);
    EXPECT_EQ(&(value /= 2.0), &value); EXPECT_EQ(value.value, 12);
    EXPECT_EQ(&(value *= std::size_t{2}), &value); EXPECT_EQ(value.value, 24);
    EXPECT_EQ(&(value /= std::size_t{2}), &value); EXPECT_EQ(value.value, 12);
}
TYPED_TEST(AngleTest, NumericConversionsReturnUnderlyingValue) {
    const TypeParam value{12.5};
    EXPECT_EQ(static_cast<double>(value), 12.5);
    EXPECT_EQ(static_cast<long double>(value), 12.5L);
    EXPECT_EQ(static_cast<float>(value), 12.5f);
    EXPECT_EQ(static_cast<int>(value), 12);
    EXPECT_EQ(static_cast<std::size_t>(value), 12u);
}
TEST(AngleConversionTest, DegreesAndRadiansRoundTripSignedAngles) {
    for (const auto degrees : {0.0L, 90.0L, 180.0L, -90.0L, 720.0L}) {
        const aho::Radian radians = aho::Degree{degrees};
        EXPECT_NEAR(static_cast<double>(radians.value), static_cast<double>(degrees / 180 * std::numbers::pi), 1e-12);
        const aho::Degree round_trip = radians;
        EXPECT_NEAR(static_cast<double>(round_trip.value), static_cast<double>(degrees), 1e-12);
    }
}
TEST(AngleLiteralTest, LiteralsPreserveUnitsAndPiMultiples) {
    using namespace aho::literals;
    EXPECT_EQ((90.0_deg).value, 90);
    EXPECT_EQ((2.0_rad).value, 2);
    EXPECT_EQ((2.0_pi).value, 2 * std::numbers::pi);
    EXPECT_EQ((2.0_π).value, 2 * std::numbers::pi);
    EXPECT_EQ(pi.value, std::numbers::pi);
}
static_assert((aho::Degree{3} + aho::Degree{4}).value == 7);
static_assert((aho::Radian{3} * std::size_t{2}).value == 6);
}
