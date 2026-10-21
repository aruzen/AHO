#include <AHO/core/color.hpp>
#include <gtest/gtest.h>
#include <cstdint>
#include <sstream>
#include <utility>
#include "../support/color.hpp"

#if defined(AHO_CONTRACT_COLOR_RGB_ARITHMETIC)
TEST(ColorContractTest, RgbArithmeticChecksValuesAndReferenceReturns) {
    using namespace aho::color;
    using aho_test::expect_rgb;
    const RGB<float> a(12, 24, 36), b(1, 2, 3);
    expect_rgb(a + b, 13, 26, 39);
    expect_rgb(a - b, 11, 22, 33);
    expect_rgb(a * 2.0f, 24, 48, 72);
    expect_rgb(a / 2.0f, 6, 12, 18);
    auto value = a;
    EXPECT_EQ(&(value += b), &value);
    expect_rgb(value, 13, 26, 39);
    EXPECT_EQ(&(value -= b), &value);
    expect_rgb(value, 12, 24, 36);
    EXPECT_EQ(&(value *= 2.0f), &value);
    expect_rgb(value, 24, 48, 72);
    EXPECT_EQ(&(value /= 2.0f), &value);
    expect_rgb(value, 12, 24, 36);
    expect_rgb(a + R<float>(1), 13, 24, 36);
    expect_rgb(a - R<float>(1), 11, 24, 36);
    expect_rgb(a + G<float>(1), 12, 25, 36);
    expect_rgb(a - G<float>(1), 12, 23, 36);
    expect_rgb(a + B<float>(1), 12, 24, 37);
    expect_rgb(a - B<float>(1), 12, 24, 35);
}
#elif defined(AHO_CONTRACT_COLOR_RGBA_ARITHMETIC)
TEST(ColorContractTest, RgbaArithmeticPreservesAlphaDuringScalarOperations) {
    using namespace aho::color;
    using aho_test::expect_rgba;
    const RGBA<float> a(12, 24, 36, 8), b(1, 2, 3, 4);
    const RGB<float> rgb(1, 2, 3);
    expect_rgba(a + b, 13, 26, 39, 12);
    expect_rgba(a - b, 11, 22, 33, 4);
    expect_rgba(a + rgb, 13, 26, 39, 8);
    expect_rgba(a - rgb, 11, 22, 33, 8);
    expect_rgba(rgb + a, 13, 26, 39, 8);
    expect_rgba(rgb + A<float>(0.5f), 1, 2, 3, 0.5);
    expect_rgba(rgb - A<float>(0.5f), 1, 2, 3, -0.5);
    expect_rgba(a * 2.0f, 24, 48, 72, 8);
    expect_rgba(a / 2.0f, 6, 12, 18, 8);
    auto value = a;
    EXPECT_EQ(&(value += b), &value);
    expect_rgba(value, 13, 26, 39, 12);
    EXPECT_EQ(&(value -= b), &value);
    expect_rgba(value, 12, 24, 36, 8);
    EXPECT_EQ(&(value += rgb), &value);
    expect_rgba(value, 13, 26, 39, 8);
    EXPECT_EQ(&(value -= rgb), &value);
    expect_rgba(value, 12, 24, 36, 8);
    EXPECT_EQ(&(value *= 2.0f), &value);
    expect_rgba(value, 24, 48, 72, 8);
    EXPECT_EQ(&(value /= 2.0f), &value);
    expect_rgba(value, 12, 24, 36, 8);
    expect_rgba(a + R<float>(1), 13, 24, 36, 8);
    expect_rgba(a - R<float>(1), 11, 24, 36, 8);
    expect_rgba(a + G<float>(1), 12, 25, 36, 8);
    expect_rgba(a - G<float>(1), 12, 23, 36, 8);
    expect_rgba(a + B<float>(1), 12, 24, 37, 8);
    expect_rgba(a - B<float>(1), 12, 24, 35, 8);
    expect_rgba(a + A<float>(1), 12, 24, 36, 9);
    expect_rgba(a - A<float>(1), 12, 24, 36, 7);
}
#elif defined(AHO_CONTRACT_COLOR_HSV_ARITHMETIC)
TEST(ColorContractTest, HsvArithmeticChecksEachChannelAndAlphaAttachment) {
    using namespace aho::color;
    using aho_test::expect_hsv;
    using aho_test::expect_hsva;
    const HSV<float, float> a(120, 0.5f, 0.25f), b(60, 0.25f, 0.125f);
    expect_hsv(a + b, 180, 0.75, 0.375);
    expect_hsv(a - b, 60, 0.25, 0.125);
    auto value = a;
    EXPECT_EQ(&(value += b), &value);
    expect_hsv(value, 180, 0.75, 0.375);
    EXPECT_EQ(&(value -= b), &value);
    expect_hsv(value, 120, 0.5, 0.25);
    expect_hsv(a + H<float>(10), 130, 0.5, 0.25);
    expect_hsv(a - H<float>(10), 110, 0.5, 0.25);
    expect_hsv(a + S<float>(0.25f), 120, 0.75, 0.25);
    expect_hsv(a - S<float>(0.25f), 120, 0.25, 0.25);
    expect_hsv(a + V<float>(0.125f), 120, 0.5, 0.375);
    expect_hsv(a - V<float>(0.125f), 120, 0.5, 0.125);
    expect_hsva(a + A<float>(0.5f), 120, 0.5, 0.25, 0.5);
    expect_hsva(a - A<float>(0.5f), 120, 0.5, 0.25, -0.5);
    expect_hsva(a + HSVA<float, float>(60, 0.25f, 0.125f, 0.5f), 180, 0.75, 0.375, 0.5);
}
#elif defined(AHO_CONTRACT_COLOR_HSVA_ARITHMETIC)
TEST(ColorContractTest, HsvaArithmeticPreservesOrCombinesAlphaAsAppropriate) {
    using namespace aho::color;
    using aho_test::expect_hsva;
    const HSVA<float, float> a(120, 0.5f, 0.25f, 0.75f), b(60, 0.25f, 0.125f, 0.25f);
    const HSV<float, float> hsv(60, 0.25f, 0.125f);
    expect_hsva(a + b, 180, 0.75, 0.375, 1);
    expect_hsva(a - b, 60, 0.25, 0.125, 0.5);
    expect_hsva(a + hsv, 180, 0.75, 0.375, 0.75);
    expect_hsva(a - hsv, 60, 0.25, 0.125, 0.75);
    auto value = a;
    EXPECT_EQ(&(value += b), &value);
    expect_hsva(value, 180, 0.75, 0.375, 1);
    EXPECT_EQ(&(value -= b), &value);
    expect_hsva(value, 120, 0.5, 0.25, 0.75);
    EXPECT_EQ(&(value += hsv), &value);
    expect_hsva(value, 180, 0.75, 0.375, 0.75);
    EXPECT_EQ(&(value -= hsv), &value);
    expect_hsva(value, 120, 0.5, 0.25, 0.75);
    expect_hsva(a + H<float>(10), 130, 0.5, 0.25, 0.75);
    expect_hsva(a - H<float>(10), 110, 0.5, 0.25, 0.75);
    expect_hsva(a + S<float>(0.25f), 120, 0.75, 0.25, 0.75);
    expect_hsva(a - S<float>(0.25f), 120, 0.25, 0.25, 0.75);
    expect_hsva(a + V<float>(0.125f), 120, 0.5, 0.375, 0.75);
    expect_hsva(a - V<float>(0.125f), 120, 0.5, 0.125, 0.75);
    expect_hsva(a + A<float>(0.125f), 120, 0.5, 0.25, 0.875);
    expect_hsva(a - A<float>(0.125f), 120, 0.5, 0.25, 0.625);
}
#elif defined(AHO_CONTRACT_COLOR_ALPHA_CONVERSIONS)
TEST(ColorContractTest, AlphaPreservingConversionsReturnCorrectSpaceAndChannels) {
    const auto hsva = aho::color::RGBA<float>(1, 0, 0, 0.5).hsva();
    const auto rgba = aho::color::HSVA<float, float>(0, 1, 1, 0.5).rgba();
    static_assert(std::same_as<decltype(hsva), const aho::color::HSVA<float, float>>);
    aho_test::expect_hsva(hsva, 0, 1, 1, 0.5);
    aho_test::expect_rgba(rgba, 1, 0, 0, 0.5);
}
#else
#include "../support/one_way_cast.hpp"
#include "../support/asymmetric_arithmetic.hpp"
#include "../support/unary_scalar.hpp"
namespace {
    using namespace aho_test::arithmetic;
    template<class T>
    class ColorChannelArithmeticConstraintTest : public testing::Test {};
    using ColorChannelArithmeticFamilies = testing::Types<Family<aho::color::R>, Family<aho::color::G>, Family<aho::color::B>, Family<aho::color::H>, Family<aho::color::S>, Family<aho::color::V>, Family<aho::color::A>>;
    TYPED_TEST_SUITE(ColorChannelArithmeticConstraintTest, ColorChannelArithmeticFamilies);
    TYPED_TEST(ColorChannelArithmeticConstraintTest, BinaryOperationsUseTheStoredScalarOnTheLeft) {
        using Left = typename TypeParam::template Of<LeftOperand>;
        using Right = typename TypeParam::template Of<Operand>;
        using Expected = typename TypeParam::template Of<int>;
        constexpr Left lhs(LeftOperand{24});
        constexpr Right rhs(Operand{4});
        static_assert(CanAdd<Left, Right>);
        static_assert(!CanAdd<Right, Left>);
        static_assert(std::same_as<decltype(lhs + rhs), Expected>);
        static_assert((lhs + rhs).value == 28);
        EXPECT_EQ((lhs + rhs).value, 28);
        static_assert(CanSubtract<Left, Right>);
        static_assert(!CanSubtract<Right, Left>);
        static_assert(std::same_as<decltype(lhs - rhs), Expected>);
        static_assert((lhs - rhs).value == 20);
        EXPECT_EQ((lhs - rhs).value, 20);
        static_assert(CanMultiply<Left, Right>);
        static_assert(!CanMultiply<Right, Left>);
        static_assert(std::same_as<decltype(lhs * rhs), Expected>);
        static_assert((lhs * rhs).value == 96);
        EXPECT_EQ((lhs * rhs).value, 96);
        static_assert(CanDivide<Left, Right>);
        static_assert(!CanDivide<Right, Left>);
        static_assert(std::same_as<decltype(lhs / rhs), Expected>);
        static_assert((lhs / rhs).value == 6);
        EXPECT_EQ((lhs / rhs).value, 6);
    }
    TYPED_TEST(ColorChannelArithmeticConstraintTest, CompoundOperationsKeepOperandOrderAndReturnTheSameWrapper) {
        using Left = typename TypeParam::template Of<LeftOperand>;
        using Right = typename TypeParam::template Of<Operand>;
        Left lhs(LeftOperand{24});
        const Right rhs(Operand{4});
        static_assert(CanAddAssign<Left, Right>);
        static_assert(!CanAddAssign<Right, Left>);
        EXPECT_EQ(&(lhs += rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 28);
        static_assert(CanSubtractAssign<Left, Right>);
        static_assert(!CanSubtractAssign<Right, Left>);
        EXPECT_EQ(&(lhs -= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 24);
        static_assert(CanMultiplyAssign<Left, Right>);
        static_assert(!CanMultiplyAssign<Right, Left>);
        EXPECT_EQ(&(lhs *= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 96);
        static_assert(CanDivideAssign<Left, Right>);
        static_assert(!CanDivideAssign<Right, Left>);
        EXPECT_EQ(&(lhs /= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 24);
    }
    TYPED_TEST(ColorChannelArithmeticConstraintTest, CompoundOperationsDoNotRequireBinaryOperators) {
        using Left = typename TypeParam::template Of<AssignmentOnly>;
        using Right = typename TypeParam::template Of<Operand>;
        Left lhs(AssignmentOnly{24});
        const Right rhs(Operand{4});
        static_assert(CanAddAssign<Left, Right>);
        static_assert(!CanAddAssign<Right, Left>);
        static_assert(!CanAdd<Left, Right>);
        EXPECT_EQ(&(lhs += rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 28);
        static_assert(CanSubtractAssign<Left, Right>);
        static_assert(!CanSubtractAssign<Right, Left>);
        static_assert(!CanSubtract<Left, Right>);
        EXPECT_EQ(&(lhs -= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 24);
        static_assert(CanMultiplyAssign<Left, Right>);
        static_assert(!CanMultiplyAssign<Right, Left>);
        static_assert(!CanMultiply<Left, Right>);
        EXPECT_EQ(&(lhs *= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 96);
        static_assert(CanDivideAssign<Left, Right>);
        static_assert(!CanDivideAssign<Right, Left>);
        static_assert(!CanDivide<Left, Right>);
        EXPECT_EQ(&(lhs /= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 24);
    }
    TYPED_TEST(ColorChannelArithmeticConstraintTest, ResultTypesMatchTheActualConstOperands) {
        using Wrapper = typename TypeParam::template Of<CategoryResult>;
        using Expected = typename TypeParam::template Of<int>;
        constexpr Wrapper lhs(CategoryResult{24}), rhs(CategoryResult{4});
        static_assert(std::same_as<decltype(lhs + rhs), Expected>);
        static_assert((lhs + rhs).value == 28);
        EXPECT_EQ((lhs + rhs).value, 28);
        static_assert(std::same_as<decltype(lhs - rhs), Expected>);
        static_assert((lhs - rhs).value == 20);
        EXPECT_EQ((lhs - rhs).value, 20);
        static_assert(std::same_as<decltype(lhs * rhs), Expected>);
        static_assert((lhs * rhs).value == 96);
        EXPECT_EQ((lhs * rhs).value, 96);
        static_assert(std::same_as<decltype(lhs / rhs), Expected>);
        static_assert((lhs / rhs).value == 6);
        EXPECT_EQ((lhs / rhs).value, 6);
    }
    TYPED_TEST(ColorChannelArithmeticConstraintTest, RejectsMutableOnlyBinaryAndMissingCompoundOperations) {
        using Mutable = typename TypeParam::template Of<MutableOnly>;
        using BinaryOnly = typename TypeParam::template Of<CategoryResult>;
        EXPECT_FALSE((CanAdd<Mutable, Mutable>));
        EXPECT_FALSE((CanAddAssign<BinaryOnly, BinaryOnly>));
        EXPECT_FALSE((CanSubtract<Mutable, Mutable>));
        EXPECT_FALSE((CanSubtractAssign<BinaryOnly, BinaryOnly>));
        EXPECT_FALSE((CanMultiply<Mutable, Mutable>));
        EXPECT_FALSE((CanMultiplyAssign<BinaryOnly, BinaryOnly>));
        EXPECT_FALSE((CanDivide<Mutable, Mutable>));
        EXPECT_FALSE((CanDivideAssign<BinaryOnly, BinaryOnly>));
    }
    struct MutableAlphaRight {
        int value{};
        auto operator<=>(const MutableAlphaRight&) const = default;
    };
    struct AlphaLeft {
        int value{};
        auto operator<=>(const AlphaLeft&) const = default;
        constexpr AlphaLeft& operator+=(MutableAlphaRight& rhs) {
            value += rhs.value;
            return *this;
        }
        constexpr AlphaLeft& operator-=(MutableAlphaRight& rhs) {
            value -= rhs.value;
            return *this;
        }
        constexpr AlphaLeft& operator*=(MutableAlphaRight& rhs) {
            value *= rhs.value;
            return *this;
        }
        constexpr AlphaLeft& operator/=(MutableAlphaRight& rhs) {
            value /= rhs.value;
            return *this;
        }
    };
    constexpr int operator+(const AlphaLeft& lhs, MutableAlphaRight& rhs) { return lhs.value + rhs.value; }
    constexpr int operator-(const AlphaLeft& lhs, MutableAlphaRight& rhs) { return lhs.value - rhs.value; }
    constexpr int operator*(const AlphaLeft& lhs, MutableAlphaRight& rhs) { return lhs.value * rhs.value; }
    constexpr int operator/(const AlphaLeft& lhs, MutableAlphaRight& rhs) { return lhs.value / rhs.value; }
    TEST(ColorAlphaArithmeticConstraintTest, BinaryOperationsUseTheMutableCopyOfTheRightOperand) {
        const aho::color::A<AlphaLeft> lhs(AlphaLeft{24});
        const aho::color::A<MutableAlphaRight> rhs(MutableAlphaRight{4});
        static_assert(CanAdd<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanAdd<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ((lhs + rhs).value, 28);
        static_assert(CanSubtract<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanSubtract<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ((lhs - rhs).value, 20);
        static_assert(CanMultiply<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanMultiply<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ((lhs * rhs).value, 96);
        static_assert(CanDivide<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanDivide<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ((lhs / rhs).value, 6);
        EXPECT_EQ(rhs.value.value, 4);
    }
    TEST(ColorAlphaArithmeticConstraintTest, CompoundOperationsUseTheMutableCopyOfTheRightOperand) {
        aho::color::A<AlphaLeft> lhs(AlphaLeft{24});
        const aho::color::A<MutableAlphaRight> rhs(MutableAlphaRight{4});
        static_assert(CanAddAssign<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanAddAssign<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ(&(lhs += rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 28);
        static_assert(CanSubtractAssign<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanSubtractAssign<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ(&(lhs -= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 24);
        static_assert(CanMultiplyAssign<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanMultiplyAssign<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ(&(lhs *= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 96);
        static_assert(CanDivideAssign<aho::color::A<AlphaLeft>, aho::color::A<MutableAlphaRight>>);
        static_assert(!CanDivideAssign<aho::color::R<AlphaLeft>, aho::color::R<MutableAlphaRight>>);
        EXPECT_EQ(&(lhs /= rhs), &lhs);
        EXPECT_EQ(lhs.value.value, 24);
        EXPECT_EQ(rhs.value.value, 4);
    }
    template<class T>
    class ColorNarrowNegationTest : public testing::Test {};
    using ColorNarrowNegationTestTypes = testing::Types<aho::color::R<std::int8_t>, aho::color::R<std::int16_t>,
                                                        aho::color::G<std::int8_t>, aho::color::G<std::int16_t>,
                                                        aho::color::B<std::int8_t>, aho::color::B<std::int16_t>,
                                                        aho::color::H<std::int8_t>, aho::color::H<std::int16_t>,
                                                        aho::color::S<std::int8_t>, aho::color::S<std::int16_t>,
                                                        aho::color::V<std::int8_t>, aho::color::V<std::int16_t>,
                                                        aho::color::A<std::int8_t>, aho::color::A<std::int16_t>>;
    TYPED_TEST_SUITE(ColorNarrowNegationTest, ColorNarrowNegationTestTypes);
    TYPED_TEST(ColorNarrowNegationTest, PromotedNegationReturnsTheDeclaredNarrowScalarType) {
        using Scalar = std::remove_cvref_t<decltype(std::declval<TypeParam>().value)>;
        constexpr TypeParam positive(Scalar{7}), negative(Scalar{-7}), zero(Scalar{0});
        static_assert(std::same_as<decltype(-positive), TypeParam>);
        static_assert((-positive).value == Scalar{-7});
        static_assert((-negative).value == Scalar{7});
        static_assert((-zero).value == Scalar{0});
        EXPECT_EQ(static_cast<int>((-positive).value), -7);
        EXPECT_EQ(static_cast<int>((-negative).value), 7);
        EXPECT_EQ(static_cast<int>((-zero).value), 0);
        EXPECT_EQ(static_cast<int>((-(-positive)).value), 7);
    }
    template<class T>
    class ColorUnaryConversionTest : public testing::Test {};
    using ColorUnaryConversionTypes = testing::Types<aho::color::R<aho_test::UnaryScalar>, aho::color::H<aho_test::UnaryScalar>, aho::color::A<aho_test::UnaryScalar>>;
    TYPED_TEST_SUITE(ColorUnaryConversionTest, ColorUnaryConversionTypes);
    TYPED_TEST(ColorUnaryConversionTest, NegationKeepsTheImplicitScalarArgumentConversion) {
        constexpr TypeParam source(aho_test::UnaryScalar{7});
        static_assert(static_cast<aho_test::UnaryScalar>(-source.value).value == 93);
        static_assert((-source).value.value == -7);
        EXPECT_EQ((-source).value.value, -7);
    }
    template<class T>
    class ColorOneWayCastTest : public testing::Test {};
    using OneWayChannels = testing::Types<aho::color::R<aho_test::CastSource>,
                                          aho::color::G<aho_test::CastSource>, aho::color::B<aho_test::CastSource>,
                                          aho::color::H<aho_test::CastSource>, aho::color::S<aho_test::CastSource>,
                                          aho::color::V<aho_test::CastSource>, aho::color::A<aho_test::CastSource>>;
    TYPED_TEST_SUITE(ColorOneWayCastTest, OneWayChannels);
    TYPED_TEST(ColorOneWayCastTest, ChecksConversionFromTheStoredScalarToTheRequestedType) {
        static_assert(aho_test::HasCast<TypeParam, aho_test::CastTarget>);
        const TypeParam source(aho_test::CastSource{23});
        const auto converted = source.template cast<aho_test::CastTarget>();
        using Converted = std::remove_cv_t<decltype(converted)>;
        static_assert(!aho_test::HasCast<Converted, aho_test::CastSource>);
        EXPECT_EQ(converted.value.value, 23);
        EXPECT_EQ(source.value.value, 23);
    }
} // namespace
namespace {
    template<class T>
    class ColorElementTest : public testing::Test {};
    using ColorElements = testing::Types<aho::color::R<double>, aho::color::G<double>, aho::color::B<double>,
                                         aho::color::H<double>, aho::color::S<double>, aho::color::V<double>, aho::color::A<double>>;
    TYPED_TEST_SUITE(ColorElementTest, ColorElements);
    TYPED_TEST(ColorElementTest, ConstructorsAndUnaryAndBinaryOperatorsPreserveValues) {
        const TypeParam a(12), b(3);
        EXPECT_EQ(TypeParam{}.value, 0);
        EXPECT_EQ((+a).value, 12);
        EXPECT_EQ((-a).value, -12);
        EXPECT_EQ((a + b).value, 15);
        EXPECT_EQ((a - b).value, 9);
        EXPECT_EQ((a * b).value, 36);
        EXPECT_EQ((a / b).value, 4);
        EXPECT_EQ(a.template cast<int>().value, 12);
    }
    TYPED_TEST(ColorElementTest, AssignmentsMutateAndReturnOriginalObject) {
        TypeParam a(12), b(3);
        EXPECT_EQ(&(a += b), &a);
        EXPECT_EQ(a.value, 15);
        EXPECT_EQ(&(a -= b), &a);
        EXPECT_EQ(a.value, 12);
        EXPECT_EQ(&(a *= b), &a);
        EXPECT_EQ(a.value, 36);
        EXPECT_EQ(&(a /= b), &a);
        EXPECT_EQ(a.value, 12);
    }
    TYPED_TEST(ColorElementTest, StreamOutputIncludesChannelNameAndValue) {
        std::ostringstream stream;
        stream << TypeParam(12);
        const auto name = [] {
            switch (TypeParam::UpperChar) {
                case 'R':
                    return "red";
                case 'G':
                    return "green";
                case 'B':
                    return "blue";
                case 'H':
                    return "hue";
                case 'S':
                    return "saturation";
                case 'V':
                    return "brightness";
                default:
                    return "alpha";
            }
        }();
        EXPECT_EQ(stream.str(), std::string(name) + " : 12");
    }
    struct PrimaryColor {
        float hue, red, green, blue;
    };
    class ColorConversionTest : public testing::TestWithParam<PrimaryColor> {};
    TEST_P(ColorConversionTest, RgbAndHsvConversionsMatchPrimaryColors) {
        const auto color = GetParam();
        aho::color::RGB<float> rgb(color.red, color.green, color.blue);
        const auto hsv = rgb.hsv();
        EXPECT_NEAR(hsv.h.value, color.hue, 1e-5);
        EXPECT_FLOAT_EQ(hsv.s.value, 1);
        EXPECT_FLOAT_EQ(hsv.v.value, 1);
        auto source = aho::color::HSV<float, float>(color.hue, 1, 1);
        const auto converted = source.rgb();
        EXPECT_FLOAT_EQ(converted.r.value, color.red);
        EXPECT_FLOAT_EQ(converted.g.value, color.green);
        EXPECT_FLOAT_EQ(converted.b.value, color.blue);
    }
    INSTANTIATE_TEST_SUITE_P(SixHueSectors, ColorConversionTest, testing::Values(PrimaryColor{0, 1, 0, 0}, PrimaryColor{60, 1, 1, 0}, PrimaryColor{120, 0, 1, 0}, PrimaryColor{180, 0, 1, 1}, PrimaryColor{240, 0, 0, 1}, PrimaryColor{300, 1, 0, 1}));
    TEST(ColorConversionTest, BlackAndGrayHaveZeroSaturationAndRoundTrip) {
        for (const float brightness : {0.0f, 0.25f, 1.0f}) {
            aho::color::RGB<float> gray(brightness, brightness, brightness);
            auto hsv = gray.hsv();
            EXPECT_FLOAT_EQ(hsv.h.value, 0);
            EXPECT_FLOAT_EQ(hsv.s.value, 0);
            EXPECT_FLOAT_EQ(hsv.v.value, brightness);
            const auto result = hsv.rgb();
            EXPECT_FLOAT_EQ(result.r.value, brightness);
            EXPECT_FLOAT_EQ(result.g.value, brightness);
            EXPECT_FLOAT_EQ(result.b.value, brightness);
        }
    }
    TEST(ColorConversionTest, IntegerChannelsUseFullRangeAndRoundBackToBytes) {
        aho::color::RGB<std::uint8_t> red(255, 0, 0);
        const auto hsv = red.hsv<short, std::uint8_t>();
        EXPECT_EQ(hsv.h.value, 0);
        EXPECT_EQ(hsv.s.value, 255);
        EXPECT_EQ(hsv.v.value, 255);
        auto source = aho::color::HSV<short, std::uint8_t>(120, 255, 255);
        const auto result = source.rgb<std::uint8_t>();
        EXPECT_EQ(result.r.value, 0);
        EXPECT_EQ(result.g.value, 255);
        EXPECT_EQ(result.b.value, 0);
    }
    template<class H, class SV>
    struct HsvaArithmeticCase {
        using Color = aho::color::HSVA<H, SV>;
        using Hue = H;
        using Channel = SV;
    };
    using HsvaArithmeticTypes = testing::Types<HsvaArithmeticCase<short, std::int8_t>, HsvaArithmeticCase<int, int>,
                                               HsvaArithmeticCase<float, float>, HsvaArithmeticCase<double, double>, HsvaArithmeticCase<short, float>>;
    template<class T>
    class HsvaArithmeticTest : public testing::Test {};
    TYPED_TEST_SUITE(HsvaArithmeticTest, HsvaArithmeticTypes);
    TYPED_TEST(HsvaArithmeticTest, AlphaCombinesWithHsvaAndStaysUnchangedWithHsv) {
        using C = typename TypeParam::Color;
        using Base = aho::color::HSV<typename TypeParam::Hue, typename TypeParam::Channel>;
        const C source(120, 12, 24, 8), delta(30, 1, 2, 3);
        const Base base(30, 1, 2);
        static_assert(std::same_as<decltype(source + delta), C>);
        aho_test::expect_hsva(source + delta, 150, 13, 26, 11);
        aho_test::expect_hsva(source - delta, 90, 11, 22, 5);
        aho_test::expect_hsva(source + base, 150, 13, 26, 8);
        aho_test::expect_hsva(source - base, 90, 11, 22, 8);
        auto changed = source;
        EXPECT_EQ(&(changed += delta), &changed);
        aho_test::expect_hsva(changed, 150, 13, 26, 11);
        EXPECT_EQ(&(changed -= delta), &changed);
        aho_test::expect_hsva(changed, 120, 12, 24, 8);
        EXPECT_EQ(&(changed += base), &changed);
        aho_test::expect_hsva(changed, 150, 13, 26, 8);
        EXPECT_EQ(&(changed -= base), &changed);
        aho_test::expect_hsva(changed, 120, 12, 24, 8);
        changed += changed;
        aho_test::expect_hsva(changed, 240, 24, 48, 16);
        changed -= changed;
        aho_test::expect_hsva(changed, 0, 0, 0, 0);
        aho_test::expect_hsva(source, 120, 12, 24, 8);
    }
    TYPED_TEST(HsvaArithmeticTest, IndividualChannelArithmeticKeepsTheOtherComponents) {
        using namespace aho::color;
        using H = typename TypeParam::Hue;
        using SV = typename TypeParam::Channel;
        const typename TypeParam::Color source(120, 12, 24, 8);
        aho_test::expect_hsva(source + aho::color::H<H>(10), 130, 12, 24, 8);
        aho_test::expect_hsva(source - aho::color::H<H>(10), 110, 12, 24, 8);
        aho_test::expect_hsva(source + S<SV>(2), 120, 14, 24, 8);
        aho_test::expect_hsva(source - S<SV>(2), 120, 10, 24, 8);
        aho_test::expect_hsva(source + V<SV>(3), 120, 12, 27, 8);
        aho_test::expect_hsva(source - V<SV>(3), 120, 12, 21, 8);
        aho_test::expect_hsva(source + A<SV>(3), 120, 12, 24, 11);
        aho_test::expect_hsva(source - A<SV>(3), 120, 12, 24, 5);
    }
    constexpr auto constexpr_hsva_sum = aho::color::HSVA<int>(1, 2, 3, 4) + aho::color::HSVA<int>(3, 2, 1, 0);
    static_assert(constexpr_hsva_sum.h.value == 4 && constexpr_hsva_sum.s.value == 4 && constexpr_hsva_sum.v.value == 4 && constexpr_hsva_sum.a.value == 4);
    TEST(ColorConstructionTest, AllColorSpacesPreserveChannelsIncludingAlpha) {
        const aho::color::RGB<int> rgb(1, 2, 3);
        EXPECT_EQ(rgb.r.value, 1);
        EXPECT_EQ(rgb.g.value, 2);
        EXPECT_EQ(rgb.b.value, 3);
        const aho::color::RGBA<int> rgba(1, 2, 3, 4);
        EXPECT_EQ(rgba.r.value, 1);
        EXPECT_EQ(rgba.g.value, 2);
        EXPECT_EQ(rgba.b.value, 3);
        EXPECT_EQ(rgba.a.value, 4);
        const aho::color::HSV<int, int> hsv(1, 2, 3);
        EXPECT_EQ(hsv.h.value, 1);
        EXPECT_EQ(hsv.s.value, 2);
        EXPECT_EQ(hsv.v.value, 3);
        const aho::color::HSVA<int, int> hsva(1, 2, 3, 4);
        EXPECT_EQ(hsva.h.value, 1);
        EXPECT_EQ(hsva.s.value, 2);
        EXPECT_EQ(hsva.v.value, 3);
        EXPECT_EQ(hsva.a.value, 4);
    }
    TEST(ColorConstructionTest, DefaultColorsInitializeEveryChannelToZero) {
        const aho::RGB rgb;
        const aho::RGBA rgba;
        const aho::HSV hsv;
        const aho::HSVA hsva;
        EXPECT_FLOAT_EQ(rgb.r.value, 0);
        EXPECT_FLOAT_EQ(rgb.g.value, 0);
        EXPECT_FLOAT_EQ(rgb.b.value, 0);
        EXPECT_FLOAT_EQ(rgba.r.value, 0);
        EXPECT_FLOAT_EQ(rgba.g.value, 0);
        EXPECT_FLOAT_EQ(rgba.b.value, 0);
        EXPECT_FLOAT_EQ(rgba.a.value, 0);
        EXPECT_FLOAT_EQ(hsv.h.value, 0);
        EXPECT_FLOAT_EQ(hsv.s.value, 0);
        EXPECT_FLOAT_EQ(hsv.v.value, 0);
        EXPECT_FLOAT_EQ(hsva.h.value, 0);
        EXPECT_FLOAT_EQ(hsva.s.value, 0);
        EXPECT_FLOAT_EQ(hsva.v.value, 0);
        EXPECT_FLOAT_EQ(hsva.a.value, 0);
    }
    template<class T>
    class RgbaArithmeticTest : public testing::Test {};
    using RgbaArithmeticScalars = testing::Types<std::int8_t, int, float, double>;
    TYPED_TEST_SUITE(RgbaArithmeticTest, RgbaArithmeticScalars);
    TYPED_TEST(RgbaArithmeticTest, ColorAndScalarArithmeticCombinesOrPreservesAlpha) {
        using T = TypeParam;
        using C = aho::color::RGBA<T>;
        const C source(12, 24, 36, 8), delta(1, 2, 3, 4);
        const aho::color::RGB<T> rgb(1, 2, 3);
        static_assert(std::same_as<decltype(source + delta), C>);
        aho_test::expect_rgba(source + delta, 13, 26, 39, 12);
        aho_test::expect_rgba(source - delta, 11, 22, 33, 4);
        aho_test::expect_rgba(source + rgb, 13, 26, 39, 8);
        aho_test::expect_rgba(source - rgb, 11, 22, 33, 8);
        aho_test::expect_rgba(source * T{2}, 24, 48, 72, 8);
        aho_test::expect_rgba(source / T{2}, 6, 12, 18, 8);
        auto changed = source;
        EXPECT_EQ(&(changed += delta), &changed);
        aho_test::expect_rgba(changed, 13, 26, 39, 12);
        EXPECT_EQ(&(changed -= delta), &changed);
        aho_test::expect_rgba(changed, 12, 24, 36, 8);
        EXPECT_EQ(&(changed += rgb), &changed);
        aho_test::expect_rgba(changed, 13, 26, 39, 8);
        EXPECT_EQ(&(changed -= rgb), &changed);
        aho_test::expect_rgba(changed, 12, 24, 36, 8);
        EXPECT_EQ(&(changed *= T{2}), &changed);
        aho_test::expect_rgba(changed, 24, 48, 72, 8);
        EXPECT_EQ(&(changed /= T{2}), &changed);
        aho_test::expect_rgba(changed, 12, 24, 36, 8);
        changed += changed;
        aho_test::expect_rgba(changed, 24, 48, 72, 16);
        changed -= changed;
        aho_test::expect_rgba(changed, 0, 0, 0, 0);
        aho_test::expect_rgba(source, 12, 24, 36, 8);
    }
    TYPED_TEST(RgbaArithmeticTest, SingleChannelChangesLeaveOtherChannelsUnchanged) {
        using namespace aho::color;
        using T = TypeParam;
        const RGBA<T> source(12, 24, 36, 8);
        aho_test::expect_rgba(source + R<T>(1), 13, 24, 36, 8);
        aho_test::expect_rgba(source - R<T>(1), 11, 24, 36, 8);
        aho_test::expect_rgba(source + G<T>(2), 12, 26, 36, 8);
        aho_test::expect_rgba(source - G<T>(2), 12, 22, 36, 8);
        aho_test::expect_rgba(source + B<T>(3), 12, 24, 39, 8);
        aho_test::expect_rgba(source - B<T>(3), 12, 24, 33, 8);
        aho_test::expect_rgba(source + A<T>(4), 12, 24, 36, 12);
        aho_test::expect_rgba(source - A<T>(4), 12, 24, 36, 4);
    }
    constexpr auto constexpr_rgba_sum = aho::color::RGBA<int>(1, 2, 3, 4) + aho::color::RGBA<int>(3, 2, 1, 0);
    static_assert(constexpr_rgba_sum.r.value == 4 && constexpr_rgba_sum.g.value == 4 && constexpr_rgba_sum.b.value == 4 && constexpr_rgba_sum.a.value == 4);
    TEST(ColorConstructionTest, ComponentAssignmentsModifyOnlyTheRequestedChannel) {
        using namespace aho::color;
        RGB<int> rgb(1, 2, 3);
        EXPECT_EQ(&(rgb += R<int>(2)), &rgb);
        EXPECT_EQ(rgb.r.value, 3);
        rgb -= R<int>(2);
        EXPECT_EQ(rgb.r.value, 1);
        rgb += G<int>(2);
        EXPECT_EQ(rgb.g.value, 4);
        rgb -= G<int>(1);
        EXPECT_EQ(rgb.g.value, 3);
        rgb += B<int>(2);
        EXPECT_EQ(rgb.b.value, 5);
        rgb -= B<int>(1);
        EXPECT_EQ(rgb.b.value, 4);
        RGBA<int> rgba(1, 2, 3, 4);
        rgba += R<int>(1);
        aho_test::expect_rgba(rgba, 2, 2, 3, 4);
        rgba -= R<int>(1);
        aho_test::expect_rgba(rgba, 1, 2, 3, 4);
        rgba += G<int>(1);
        aho_test::expect_rgba(rgba, 1, 3, 3, 4);
        rgba -= G<int>(1);
        aho_test::expect_rgba(rgba, 1, 2, 3, 4);
        rgba += B<int>(1);
        aho_test::expect_rgba(rgba, 1, 2, 4, 4);
        rgba -= B<int>(1);
        aho_test::expect_rgba(rgba, 1, 2, 3, 4);
        EXPECT_EQ(&(rgba += A<int>(2)), &rgba);
        EXPECT_EQ(rgba.a.value, 6);
        rgba -= A<int>(1);
        EXPECT_EQ(rgba.a.value, 5);
        HSV<int, int> hsv(1, 2, 3);
        hsv += H<int>(1);
        aho_test::expect_hsv(hsv, 2, 2, 3);
        hsv -= H<int>(1);
        aho_test::expect_hsv(hsv, 1, 2, 3);
        hsv += S<int>(1);
        aho_test::expect_hsv(hsv, 1, 3, 3);
        hsv -= S<int>(1);
        aho_test::expect_hsv(hsv, 1, 2, 3);
        hsv += V<int>(1);
        aho_test::expect_hsv(hsv, 1, 2, 4);
        hsv -= V<int>(1);
        aho_test::expect_hsv(hsv, 1, 2, 3);
        EXPECT_EQ(hsv.h.value, 1);
        EXPECT_EQ(hsv.s.value, 2);
        EXPECT_EQ(hsv.v.value, 3);
        HSVA<int, int> hsva(1, 2, 3, 4);
        hsva += H<int>(1);
        aho_test::expect_hsva(hsva, 2, 2, 3, 4);
        hsva -= H<int>(1);
        aho_test::expect_hsva(hsva, 1, 2, 3, 4);
        hsva += S<int>(1);
        aho_test::expect_hsva(hsva, 1, 3, 3, 4);
        hsva -= S<int>(1);
        aho_test::expect_hsva(hsva, 1, 2, 3, 4);
        hsva += V<int>(1);
        aho_test::expect_hsva(hsva, 1, 2, 4, 4);
        hsva -= V<int>(1);
        aho_test::expect_hsva(hsva, 1, 2, 3, 4);
        hsva += A<int>(1);
        aho_test::expect_hsva(hsva, 1, 2, 3, 5);
        hsva -= A<int>(1);
        aho_test::expect_hsva(hsva, 1, 2, 3, 4);
    }
    template<class H, class SV>
    struct HsvArithmeticCase {
        using Color = aho::color::HSV<H, SV>;
        using Hue = H;
        using Channel = SV;
    };
    using HsvArithmeticTypes = testing::Types<HsvArithmeticCase<short, std::int8_t>, HsvArithmeticCase<int, int>,
                                              HsvArithmeticCase<float, float>, HsvArithmeticCase<double, double>, HsvArithmeticCase<short, float>>;
    template<class T>
    class HsvArithmeticTest : public testing::Test {};
    TYPED_TEST_SUITE(HsvArithmeticTest, HsvArithmeticTypes);
    TYPED_TEST(HsvArithmeticTest, ArithmeticPreservesSeparateChannelTypesAndOriginalValues) {
        using C = typename TypeParam::Color;
        const C source(120, 12, 24), delta(30, 1, 2);
        static_assert(std::same_as<decltype(source + delta), C>);
        aho_test::expect_hsv(source + delta, 150, 13, 26);
        aho_test::expect_hsv(source - delta, 90, 11, 22);
        auto changed = source;
        EXPECT_EQ(&(changed += delta), &changed);
        aho_test::expect_hsv(changed, 150, 13, 26);
        EXPECT_EQ(&(changed -= delta), &changed);
        aho_test::expect_hsv(changed, 120, 12, 24);
        changed += changed;
        aho_test::expect_hsv(changed, 240, 24, 48);
        changed -= changed;
        aho_test::expect_hsv(changed, 0, 0, 0);
        aho_test::expect_hsv(source, 120, 12, 24);
    }
    TYPED_TEST(HsvArithmeticTest, IndividualChannelsAndAlphaKeepUnchangedComponents) {
        using namespace aho::color;
        using H = typename TypeParam::Hue;
        using SV = typename TypeParam::Channel;
        const typename TypeParam::Color source(120, 12, 24);
        aho_test::expect_hsv(source + aho::color::H<H>(10), 130, 12, 24);
        aho_test::expect_hsv(source - aho::color::H<H>(10), 110, 12, 24);
        aho_test::expect_hsv(source + S<SV>(2), 120, 14, 24);
        aho_test::expect_hsv(source - S<SV>(2), 120, 10, 24);
        aho_test::expect_hsv(source + V<SV>(3), 120, 12, 27);
        aho_test::expect_hsv(source - V<SV>(3), 120, 12, 21);
        aho_test::expect_hsva(source + A<SV>(5), 120, 12, 24, 5);
        aho_test::expect_hsva(source - A<SV>(5), 120, 12, 24, -5);
        aho_test::expect_hsva(source + HSVA<H, SV>(30, 1, 2, 5), 150, 13, 26, 5);
    }
    constexpr auto constexpr_hsv_sum = aho::color::HSV<int>(1, 2, 3) + aho::color::HSV<int>(3, 2, 1);
    static_assert(constexpr_hsv_sum.h.value == 4 && constexpr_hsv_sum.s.value == 4 && constexpr_hsv_sum.v.value == 4);
    TEST(ColorLiteralTest, ChannelLiteralsPreserveValuesAndTypes) {
        using namespace aho::literals;
        EXPECT_EQ((2_r).value, 2);
        EXPECT_EQ((3_g).value, 3);
        EXPECT_EQ((4_b).value, 4);
        EXPECT_EQ((5_h).value, 5);
        EXPECT_EQ((6_s).value, 6);
        EXPECT_EQ((7_v).value, 7);
        EXPECT_EQ((8_a).value, 8);
        EXPECT_EQ((0.5_r).value, 0.5);
        EXPECT_EQ((0.5_f_g).value, 0.5f);
        EXPECT_EQ((2_l_b).value, 2u);
        EXPECT_EQ((0.5_g).value, 0.5);
        EXPECT_EQ((0.5_b).value, 0.5);
        EXPECT_EQ((0.5_h).value, 0.5);
        EXPECT_EQ((0.5_s).value, 0.5);
        EXPECT_EQ((0.5_v).value, 0.5);
        EXPECT_EQ((0.5_a).value, 0.5);
        EXPECT_EQ((0.5_f_r).value, 0.5f);
        EXPECT_EQ((0.5_f_b).value, 0.5f);
        EXPECT_EQ((0.5_f_h).value, 0.5f);
        EXPECT_EQ((0.5_f_s).value, 0.5f);
        EXPECT_EQ((0.5_f_v).value, 0.5f);
        EXPECT_EQ((0.5_f_a).value, 0.5f);
        EXPECT_EQ((2_l_r).value, 2u);
        EXPECT_EQ((2_l_g).value, 2u);
        EXPECT_EQ((2_l_h).value, 2u);
        EXPECT_EQ((2_l_s).value, 2u);
        EXPECT_EQ((2_l_v).value, 2u);
        EXPECT_EQ((2_l_a).value, 2u);
    }
    template<class T>
    class RgbArithmeticTest : public testing::Test {};
    using RgbArithmeticScalars = testing::Types<std::int8_t, int, float, double>;
    TYPED_TEST_SUITE(RgbArithmeticTest, RgbArithmeticScalars);
    TYPED_TEST(RgbArithmeticTest, ArithmeticAndCompoundUpdatesPreserveElementTypeAndSource) {
        using T = TypeParam;
        using C = aho::color::RGB<T>;
        const C source(12, 24, 36), delta(1, 2, 3);
        static_assert(std::same_as<decltype(source + delta), C>);
        static_assert(std::same_as<decltype(source * T{2}), C>);
        aho_test::expect_rgb(source + delta, 13, 26, 39);
        aho_test::expect_rgb(source - delta, 11, 22, 33);
        aho_test::expect_rgb(source * T{2}, 24, 48, 72);
        aho_test::expect_rgb(source / T{2}, 6, 12, 18);
        auto changed = source;
        EXPECT_EQ(&(changed += delta), &changed);
        aho_test::expect_rgb(changed, 13, 26, 39);
        EXPECT_EQ(&(changed -= delta), &changed);
        aho_test::expect_rgb(changed, 12, 24, 36);
        EXPECT_EQ(&(changed *= T{2}), &changed);
        aho_test::expect_rgb(changed, 24, 48, 72);
        EXPECT_EQ(&(changed /= T{2}), &changed);
        aho_test::expect_rgb(changed, 12, 24, 36);
        aho_test::expect_rgb(source, 12, 24, 36);
    }
    TYPED_TEST(RgbArithmeticTest, ChannelUpdatesAndAlphaAttachmentUseNumericValues) {
        using namespace aho::color;
        using T = TypeParam;
        const RGB<T> source(12, 24, 36);
        aho_test::expect_rgb(source + R<T>(1), 13, 24, 36);
        aho_test::expect_rgb(source - R<T>(1), 11, 24, 36);
        aho_test::expect_rgb(source + G<T>(2), 12, 26, 36);
        aho_test::expect_rgb(source - G<T>(2), 12, 22, 36);
        aho_test::expect_rgb(source + B<T>(3), 12, 24, 39);
        aho_test::expect_rgb(source - B<T>(3), 12, 24, 33);
        aho_test::expect_rgba(source + A<T>(5), 12, 24, 36, 5);
        aho_test::expect_rgba(source - A<T>(5), 12, 24, 36, -5);
        aho_test::expect_rgba(source + RGBA<T>(1, 2, 3, 5), 13, 26, 39, 5);
    }
    constexpr auto constexpr_rgb_sum = aho::color::RGB<int>(1, 2, 3) + aho::color::RGB<int>(3, 2, 1);
    static_assert(constexpr_rgb_sum.r.value == 4 && constexpr_rgb_sum.g.value == 4 && constexpr_rgb_sum.b.value == 4);
    TEST(ColorCompositionTest, IndependentChannelsComposeWithoutChangingOtherChannels) {
        using namespace aho::color;
        const auto rgb = R<double>(2) + G<double>(3);
        EXPECT_EQ(rgb.r.value, 2);
        EXPECT_EQ(rgb.g.value, 3);
        EXPECT_EQ(rgb.b.value, 0);
        const auto rb = R<double>(2) + B<double>(4);
        EXPECT_EQ(rb.r.value, 2);
        EXPECT_EQ(rb.g.value, 0);
        EXPECT_EQ(rb.b.value, 4);
        const auto hsv = H<double>(120) + S<double>(0.5);
        EXPECT_EQ(hsv.h.value, 120);
        EXPECT_EQ(hsv.s.value, 0.5);
        EXPECT_EQ(hsv.v.value, 0);
        const auto rgba = A<double>(0.5) + R<double>(2);
        EXPECT_EQ(rgba.r.value, 2);
        EXPECT_EQ(rgba.a.value, 0.5);
        const auto hsva = A<double>(0.5) + H<double>(120);
        EXPECT_EQ(hsva.h.value, 120);
        EXPECT_EQ(hsva.a.value, 0.5);
        aho_test::expect_rgba(R<double>(2) + A<double>(0.5), 2, 0, 0, 0.5);
        aho_test::expect_hsva(H<double>(120) + A<double>(0.5), 120, 0, 0, 0.5);
    }
    TEST(ColorAlphaConversionTest, ExplicitIntegerTypesRoundTripAllChannels) {
        aho::color::RGBA<std::uint8_t> rgba(255, 0, 0, 128);
        auto hsva = rgba.hsva<short, std::uint8_t>();
        static_assert(std::same_as<decltype(hsva), aho::color::HSVA<short, std::uint8_t>>);
        aho_test::expect_hsva(hsva, 0, 255, 255, 128);
        const auto restored = hsva.rgba<std::uint8_t>();
        static_assert(std::same_as<std::remove_cv_t<decltype(restored)>, aho::color::RGBA<std::uint8_t>>);
        aho_test::expect_rgba(restored, 255, 0, 0, 128);
    }
    TEST(ColorAlphaConversionTest, ExplicitDoubleTypesKeepAlphaPrecision) {
        constexpr double alpha = 0.123456789012345;
        aho::color::RGBA<double> source(0, 1, 0, alpha);
        auto hsva = source.hsva<double, double>();
        static_assert(std::same_as<decltype(hsva), aho::color::HSVA<double, double>>);
        aho_test::expect_hsva(hsva, 120, 1, 1, alpha);
        const auto restored = hsva.rgba<double>();
        aho_test::expect_rgba(restored, 0, 1, 0, alpha);
        EXPECT_DOUBLE_EQ(source.a.value, alpha);
    }
    TEST(ColorAlphaConversionTest, TransparentAndOpaqueEndpointsSurviveBothDirections) {
        for (float alpha : {0.0f, 1.0f}) {
            aho::color::RGBA<float> rgba(1, 0, 0, alpha);
            auto hsva = rgba.hsva();
            aho_test::expect_hsva(hsva, 0, 1, 1, alpha);
            aho_test::expect_rgba(hsva.rgba(), 1, 0, 0, alpha);
        }
    }
    TEST(ColorAlphaConversionTest, FloatAlphaRoundsToTheNearestIntegerChannel) {
        for (const auto [alpha, expected] : {std::pair{0.25, 64}, {0.5, 128}, {0.75, 191}}) {
            aho::color::HSVA<double, double> hsva(120, 1, 1, alpha);
            aho::color::RGBA<double> rgba(0, 1, 0, alpha);
            aho_test::expect_rgba(hsva.rgba<std::uint8_t>(), 0, 255, 0, expected);
            aho_test::expect_hsva(rgba.hsva<short, std::uint8_t>(), 120, 255, 255, expected);
        }
    }
    TEST(ColorAlphaConversionTest, IntegerAlphaIsNormalizedForFloatingPointOutput) {
        aho::color::RGBA<std::uint8_t> rgba(255, 0, 0, 128);
        const auto hsva = rgba.hsva<double, double>();
        aho_test::expect_hsva(hsva, 0, 1, 1, 128.0 / 255);
        aho::color::HSVA<short, std::uint8_t> source(0, 255, 255, 128);
        aho_test::expect_rgba(source.rgba<double>(), 1, 0, 0, 128.0 / 255);
    }
    TEST(ColorCompositionTest, ChannelSubtractionPreservesLeftOperandSign) {
        using namespace aho::color;
        const auto rgb = R<double>(2) - G<double>(3);
        EXPECT_EQ(rgb.r.value, 2);
        EXPECT_EQ(rgb.g.value, -3);
        EXPECT_EQ(rgb.b.value, 0);
        const auto hsv = H<double>(120) - S<double>(0.5);
        EXPECT_EQ(hsv.h.value, 120);
        EXPECT_EQ(hsv.s.value, -0.5);
        aho_test::expect_rgba(R<double>(2) - A<double>(0.5), 2, 0, 0, -0.5);
        aho_test::expect_rgba(A<double>(0.5) - R<double>(2), -2, 0, 0, 0.5);
        aho_test::expect_hsva(H<double>(120) - A<double>(0.5), 120, 0, 0, -0.5);
        aho_test::expect_hsva(A<double>(0.5) - H<double>(120), -120, 0, 0, 0.5);
    }
    template<class T>
    class ColorChannelSubtractionTest : public testing::Test {};
    using SignedChannelTypes = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(ColorChannelSubtractionTest, SignedChannelTypes);
    TYPED_TEST(ColorChannelSubtractionTest, RgbAndHsvPairsPreserveTheLeftChannelAndNegateTheRight) {
        using namespace aho::color;
        using T = TypeParam;
        aho_test::expect_rgb(R<T>(2) - G<T>(3), 2, -3, 0);
        aho_test::expect_rgb(G<T>(3) - R<T>(2), -2, 3, 0);
        aho_test::expect_rgb(R<T>(2) - B<T>(4), 2, 0, -4);
        aho_test::expect_rgb(B<T>(4) - R<T>(2), -2, 0, 4);
        aho_test::expect_rgb(G<T>(3) - B<T>(4), 0, 3, -4);
        aho_test::expect_rgb(B<T>(4) - G<T>(3), 0, -3, 4);
        aho_test::expect_hsv(H<T>(2) - S<T>(3), 2, -3, 0);
        aho_test::expect_hsv(S<T>(3) - H<T>(2), -2, 3, 0);
        aho_test::expect_hsv(H<T>(2) - V<T>(4), 2, 0, -4);
        aho_test::expect_hsv(V<T>(4) - H<T>(2), -2, 0, 4);
        aho_test::expect_hsv(S<T>(3) - V<T>(4), 0, 3, -4);
        aho_test::expect_hsv(V<T>(4) - S<T>(3), 0, -3, 4);
    }
    TYPED_TEST(ColorChannelSubtractionTest, AlphaPairsKeepOperandSignsInBothOrders) {
        using namespace aho::color;
        using T = TypeParam;
        aho_test::expect_rgba(R<T>(2) - A<T>(5), 2, 0, 0, -5);
        aho_test::expect_rgba(A<T>(5) - R<T>(2), -2, 0, 0, 5);
        aho_test::expect_rgba(G<T>(3) - A<T>(5), 0, 3, 0, -5);
        aho_test::expect_rgba(A<T>(5) - G<T>(3), 0, -3, 0, 5);
        aho_test::expect_rgba(B<T>(4) - A<T>(5), 0, 0, 4, -5);
        aho_test::expect_rgba(A<T>(5) - B<T>(4), 0, 0, -4, 5);
        aho_test::expect_hsva(H<T>(2) - A<T>(5), 2, 0, 0, -5);
        aho_test::expect_hsva(A<T>(5) - H<T>(2), -2, 0, 0, 5);
        aho_test::expect_hsva(S<T>(3) - A<T>(5), 0, 3, 0, -5);
        aho_test::expect_hsva(A<T>(5) - S<T>(3), 0, -3, 0, 5);
        aho_test::expect_hsva(V<T>(4) - A<T>(5), 0, 0, 4, -5);
        aho_test::expect_hsva(A<T>(5) - V<T>(4), 0, 0, -4, 5);
    }
    TEST(ColorOutputTest, AllColorSpaceOutputsNameAndPreserveEveryChannel) {
        auto output = [](const auto& color) { std::ostringstream stream; stream << color; return stream.str(); };
        EXPECT_EQ(output(aho::color::RGB<int>(1, 2, 3)), "(r, g, b)=(1, 2, 3)");
        EXPECT_EQ(output(aho::color::RGBA<int>(1, 2, 3, 4)), "(r, g, b, a)=(1, 2, 3, 4)");
        EXPECT_EQ(output(aho::color::HSV<int, int>(1, 2, 3)), "(h, s, v)=(1, 2, 3)");
        EXPECT_EQ(output(aho::color::HSVA<int, int>(1, 2, 3, 4)), "(h, s, v, a)=(1, 2, 3, 4)");
    }
} // namespace
#endif
