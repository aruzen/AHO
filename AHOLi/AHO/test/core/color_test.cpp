#include <gtest/gtest.h>
#include <VSL/format.hpp>
#include <AHO/core/color.hpp>
#include <cstdint>
#include <sstream>
#include "../support/color.hpp"

#if defined(AHO_CONTRACT_COLOR_RGB_ARITHMETIC)
TEST(ColorContractTest, RgbArithmeticChecksValuesAndReferenceReturns) {
    using namespace aho::color;
    using aho_test::expect_rgb;
    const RGB<float> a(12, 24, 36), b(1, 2, 3);
    expect_rgb(a + b, 13, 26, 39); expect_rgb(a - b, 11, 22, 33);
    expect_rgb(a * 2.0f, 24, 48, 72); expect_rgb(a / 2.0f, 6, 12, 18);
    auto value = a;
    EXPECT_EQ(&(value += b), &value); expect_rgb(value, 13, 26, 39);
    EXPECT_EQ(&(value -= b), &value); expect_rgb(value, 12, 24, 36);
    EXPECT_EQ(&(value *= 2.0f), &value); expect_rgb(value, 24, 48, 72);
    EXPECT_EQ(&(value /= 2.0f), &value); expect_rgb(value, 12, 24, 36);
    expect_rgb(a + R<float>(1), 13, 24, 36); expect_rgb(a - R<float>(1), 11, 24, 36);
    expect_rgb(a + G<float>(1), 12, 25, 36); expect_rgb(a - G<float>(1), 12, 23, 36);
    expect_rgb(a + B<float>(1), 12, 24, 37); expect_rgb(a - B<float>(1), 12, 24, 35);
}
#elif defined(AHO_CONTRACT_COLOR_RGBA_ARITHMETIC)
TEST(ColorContractTest, RgbaArithmeticPreservesAlphaDuringScalarOperations) {
    using namespace aho::color;
    using aho_test::expect_rgba;
    const RGBA<float> a(12, 24, 36, 8), b(1, 2, 3, 4); const RGB<float> rgb(1, 2, 3);
    expect_rgba(a + b, 13, 26, 39, 12); expect_rgba(a - b, 11, 22, 33, 4);
    expect_rgba(a + rgb, 13, 26, 39, 8); expect_rgba(a - rgb, 11, 22, 33, 8);
    expect_rgba(rgb + a, 13, 26, 39, 8);
    expect_rgba(rgb + A<float>(0.5f), 1, 2, 3, 0.5); expect_rgba(rgb - A<float>(0.5f), 1, 2, 3, -0.5);
    expect_rgba(a * 2.0f, 24, 48, 72, 8); expect_rgba(a / 2.0f, 6, 12, 18, 8);
    auto value = a;
    EXPECT_EQ(&(value += b), &value); expect_rgba(value, 13, 26, 39, 12);
    EXPECT_EQ(&(value -= b), &value); expect_rgba(value, 12, 24, 36, 8);
    EXPECT_EQ(&(value += rgb), &value); expect_rgba(value, 13, 26, 39, 8);
    EXPECT_EQ(&(value -= rgb), &value); expect_rgba(value, 12, 24, 36, 8);
    EXPECT_EQ(&(value *= 2.0f), &value); expect_rgba(value, 24, 48, 72, 8);
    EXPECT_EQ(&(value /= 2.0f), &value); expect_rgba(value, 12, 24, 36, 8);
    expect_rgba(a + R<float>(1), 13, 24, 36, 8); expect_rgba(a - R<float>(1), 11, 24, 36, 8);
    expect_rgba(a + G<float>(1), 12, 25, 36, 8); expect_rgba(a - G<float>(1), 12, 23, 36, 8);
    expect_rgba(a + B<float>(1), 12, 24, 37, 8); expect_rgba(a - B<float>(1), 12, 24, 35, 8);
    expect_rgba(a + A<float>(1), 12, 24, 36, 9); expect_rgba(a - A<float>(1), 12, 24, 36, 7);
}
#elif defined(AHO_CONTRACT_COLOR_HSV_ARITHMETIC)
TEST(ColorContractTest, HsvArithmeticChecksEachChannelAndAlphaAttachment) {
    using namespace aho::color;
    using aho_test::expect_hsv; using aho_test::expect_hsva;
    const HSV<float, float> a(120, 0.5f, 0.25f), b(60, 0.25f, 0.125f);
    expect_hsv(a + b, 180, 0.75, 0.375); expect_hsv(a - b, 60, 0.25, 0.125);
    auto value = a;
    EXPECT_EQ(&(value += b), &value); expect_hsv(value, 180, 0.75, 0.375);
    EXPECT_EQ(&(value -= b), &value); expect_hsv(value, 120, 0.5, 0.25);
    expect_hsv(a + H<float>(10), 130, 0.5, 0.25); expect_hsv(a - H<float>(10), 110, 0.5, 0.25);
    expect_hsv(a + S<float>(0.25f), 120, 0.75, 0.25); expect_hsv(a - S<float>(0.25f), 120, 0.25, 0.25);
    expect_hsv(a + V<float>(0.125f), 120, 0.5, 0.375); expect_hsv(a - V<float>(0.125f), 120, 0.5, 0.125);
    expect_hsva(a + A<float>(0.5f), 120, 0.5, 0.25, 0.5); expect_hsva(a - A<float>(0.5f), 120, 0.5, 0.25, -0.5);
    expect_hsva(a + HSVA<float, float>(60, 0.25f, 0.125f, 0.5f), 180, 0.75, 0.375, 0.5);
}
#elif defined(AHO_CONTRACT_COLOR_HSVA_ARITHMETIC)
TEST(ColorContractTest, HsvaArithmeticPreservesOrCombinesAlphaAsAppropriate) {
    using namespace aho::color;
    using aho_test::expect_hsva;
    const HSVA<float, float> a(120, 0.5f, 0.25f, 0.75f), b(60, 0.25f, 0.125f, 0.25f);
    const HSV<float, float> hsv(60, 0.25f, 0.125f);
    expect_hsva(a + b, 180, 0.75, 0.375, 1); expect_hsva(a - b, 60, 0.25, 0.125, 0.5);
    expect_hsva(a + hsv, 180, 0.75, 0.375, 0.75); expect_hsva(a - hsv, 60, 0.25, 0.125, 0.75);
    auto value = a;
    EXPECT_EQ(&(value += b), &value); expect_hsva(value, 180, 0.75, 0.375, 1);
    EXPECT_EQ(&(value -= b), &value); expect_hsva(value, 120, 0.5, 0.25, 0.75);
    EXPECT_EQ(&(value += hsv), &value); expect_hsva(value, 180, 0.75, 0.375, 0.75);
    EXPECT_EQ(&(value -= hsv), &value); expect_hsva(value, 120, 0.5, 0.25, 0.75);
    expect_hsva(a + H<float>(10), 130, 0.5, 0.25, 0.75); expect_hsva(a - H<float>(10), 110, 0.5, 0.25, 0.75);
    expect_hsva(a + S<float>(0.25f), 120, 0.75, 0.25, 0.75); expect_hsva(a - S<float>(0.25f), 120, 0.25, 0.25, 0.75);
    expect_hsva(a + V<float>(0.125f), 120, 0.5, 0.375, 0.75); expect_hsva(a - V<float>(0.125f), 120, 0.5, 0.125, 0.75);
    expect_hsva(a + A<float>(0.125f), 120, 0.5, 0.25, 0.875); expect_hsva(a - A<float>(0.125f), 120, 0.5, 0.25, 0.625);
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
namespace {
template<class T> class ColorElementTest : public testing::Test {};
using ColorElements = testing::Types<aho::color::R<double>, aho::color::G<double>, aho::color::B<double>,
    aho::color::H<double>, aho::color::S<double>, aho::color::V<double>, aho::color::A<double>>;
TYPED_TEST_SUITE(ColorElementTest, ColorElements);
TYPED_TEST(ColorElementTest, ConstructorsAndUnaryAndBinaryOperatorsPreserveValues) {
    const TypeParam a(12), b(3);
    EXPECT_EQ(TypeParam{}.value, 0);
    EXPECT_EQ((+a).value, 12); EXPECT_EQ((-a).value, -12);
    EXPECT_EQ((a + b).value, 15); EXPECT_EQ((a - b).value, 9);
    EXPECT_EQ((a * b).value, 36); EXPECT_EQ((a / b).value, 4);
    EXPECT_EQ(a.template cast<int>().value, 12);
}
TYPED_TEST(ColorElementTest, AssignmentsMutateAndReturnOriginalObject) {
    TypeParam a(12), b(3);
    EXPECT_EQ(&(a += b), &a); EXPECT_EQ(a.value, 15);
    EXPECT_EQ(&(a -= b), &a); EXPECT_EQ(a.value, 12);
    EXPECT_EQ(&(a *= b), &a); EXPECT_EQ(a.value, 36);
    EXPECT_EQ(&(a /= b), &a); EXPECT_EQ(a.value, 12);
}
TYPED_TEST(ColorElementTest, StreamOutputIncludesChannelNameAndValue) {
    std::ostringstream stream;
    stream << TypeParam(12);
    const auto name = [] {
        switch (TypeParam::UpperChar) {
            case 'R': return "red"; case 'G': return "green"; case 'B': return "blue";
            case 'H': return "hue"; case 'S': return "saturation"; case 'V': return "brightness";
            default: return "alpha";
        }
    }();
    EXPECT_EQ(stream.str(), std::string(name) + " : 12");
}
struct PrimaryColor { float hue, red, green, blue; };
class ColorConversionTest : public testing::TestWithParam<PrimaryColor> {};
TEST_P(ColorConversionTest, RgbAndHsvConversionsMatchPrimaryColors) {
    const auto color = GetParam();
    aho::color::RGB<float> rgb(color.red, color.green, color.blue);
    const auto hsv = rgb.hsv();
    EXPECT_NEAR(hsv.h.value, color.hue, 1e-5);
    EXPECT_FLOAT_EQ(hsv.s.value, 1); EXPECT_FLOAT_EQ(hsv.v.value, 1);
    auto source = aho::color::HSV<float, float>(color.hue, 1, 1);
    const auto converted = source.rgb();
    EXPECT_FLOAT_EQ(converted.r.value, color.red);
    EXPECT_FLOAT_EQ(converted.g.value, color.green);
    EXPECT_FLOAT_EQ(converted.b.value, color.blue);
}
INSTANTIATE_TEST_SUITE_P(SixHueSectors, ColorConversionTest, testing::Values(
    PrimaryColor{0, 1, 0, 0}, PrimaryColor{60, 1, 1, 0}, PrimaryColor{120, 0, 1, 0},
    PrimaryColor{180, 0, 1, 1}, PrimaryColor{240, 0, 0, 1}, PrimaryColor{300, 1, 0, 1}));
TEST(ColorConversionTest, BlackAndGrayHaveZeroSaturationAndRoundTrip) {
    for (const float brightness : {0.0f, 0.25f, 1.0f}) {
        aho::color::RGB<float> gray(brightness, brightness, brightness);
        auto hsv = gray.hsv();
        EXPECT_FLOAT_EQ(hsv.h.value, 0); EXPECT_FLOAT_EQ(hsv.s.value, 0); EXPECT_FLOAT_EQ(hsv.v.value, brightness);
        const auto result = hsv.rgb();
        EXPECT_FLOAT_EQ(result.r.value, brightness); EXPECT_FLOAT_EQ(result.g.value, brightness); EXPECT_FLOAT_EQ(result.b.value, brightness);
    }
}
TEST(ColorConversionTest, IntegerChannelsUseFullRangeAndRoundBackToBytes) {
    aho::color::RGB<std::uint8_t> red(255, 0, 0);
    const auto hsv = red.hsv<short, std::uint8_t>();
    EXPECT_EQ(hsv.h.value, 0); EXPECT_EQ(hsv.s.value, 255); EXPECT_EQ(hsv.v.value, 255);
    auto source = aho::color::HSV<short, std::uint8_t>(120, 255, 255);
    const auto result = source.rgb<std::uint8_t>();
    EXPECT_EQ(result.r.value, 0); EXPECT_EQ(result.g.value, 255); EXPECT_EQ(result.b.value, 0);
}
TEST(ColorConstructionTest, AllColorSpacesPreserveChannelsIncludingAlpha) {
    const aho::color::RGB<int> rgb(1, 2, 3);
    EXPECT_EQ(rgb.r.value, 1); EXPECT_EQ(rgb.g.value, 2); EXPECT_EQ(rgb.b.value, 3);
    const aho::color::RGBA<int> rgba(1, 2, 3, 4);
    EXPECT_EQ(rgba.r.value, 1); EXPECT_EQ(rgba.g.value, 2); EXPECT_EQ(rgba.b.value, 3); EXPECT_EQ(rgba.a.value, 4);
    const aho::color::HSV<int, int> hsv(1, 2, 3);
    EXPECT_EQ(hsv.h.value, 1); EXPECT_EQ(hsv.s.value, 2); EXPECT_EQ(hsv.v.value, 3);
    const aho::color::HSVA<int, int> hsva(1, 2, 3, 4);
    EXPECT_EQ(hsva.h.value, 1); EXPECT_EQ(hsva.s.value, 2); EXPECT_EQ(hsva.v.value, 3); EXPECT_EQ(hsva.a.value, 4);
}
TEST(ColorConstructionTest, DefaultColorsInitializeEveryChannelToZero) {
    const aho::RGB rgb;
    const aho::RGBA rgba;
    const aho::HSV hsv;
    const aho::HSVA hsva;
    EXPECT_FLOAT_EQ(rgb.r.value, 0); EXPECT_FLOAT_EQ(rgb.g.value, 0); EXPECT_FLOAT_EQ(rgb.b.value, 0);
    EXPECT_FLOAT_EQ(rgba.r.value, 0); EXPECT_FLOAT_EQ(rgba.g.value, 0); EXPECT_FLOAT_EQ(rgba.b.value, 0); EXPECT_FLOAT_EQ(rgba.a.value, 0);
    EXPECT_FLOAT_EQ(hsv.h.value, 0); EXPECT_FLOAT_EQ(hsv.s.value, 0); EXPECT_FLOAT_EQ(hsv.v.value, 0);
    EXPECT_FLOAT_EQ(hsva.h.value, 0); EXPECT_FLOAT_EQ(hsva.s.value, 0); EXPECT_FLOAT_EQ(hsva.v.value, 0); EXPECT_FLOAT_EQ(hsva.a.value, 0);
}
template<class T> class RgbaArithmeticTest : public testing::Test {};
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
    EXPECT_EQ(&(changed += delta), &changed); aho_test::expect_rgba(changed, 13, 26, 39, 12);
    EXPECT_EQ(&(changed -= delta), &changed); aho_test::expect_rgba(changed, 12, 24, 36, 8);
    EXPECT_EQ(&(changed += rgb), &changed); aho_test::expect_rgba(changed, 13, 26, 39, 8);
    EXPECT_EQ(&(changed -= rgb), &changed); aho_test::expect_rgba(changed, 12, 24, 36, 8);
    EXPECT_EQ(&(changed *= T{2}), &changed); aho_test::expect_rgba(changed, 24, 48, 72, 8);
    EXPECT_EQ(&(changed /= T{2}), &changed); aho_test::expect_rgba(changed, 12, 24, 36, 8);
    changed += changed; aho_test::expect_rgba(changed, 24, 48, 72, 16);
    changed -= changed; aho_test::expect_rgba(changed, 0, 0, 0, 0);
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
    EXPECT_EQ(&(rgb += R<int>(2)), &rgb); EXPECT_EQ(rgb.r.value, 3);
    rgb -= R<int>(2); EXPECT_EQ(rgb.r.value, 1);
    rgb += G<int>(2); EXPECT_EQ(rgb.g.value, 4); rgb -= G<int>(1); EXPECT_EQ(rgb.g.value, 3);
    rgb += B<int>(2); EXPECT_EQ(rgb.b.value, 5); rgb -= B<int>(1); EXPECT_EQ(rgb.b.value, 4);
    RGBA<int> rgba(1, 2, 3, 4);
    rgba += R<int>(1); aho_test::expect_rgba(rgba, 2, 2, 3, 4);
    rgba -= R<int>(1); aho_test::expect_rgba(rgba, 1, 2, 3, 4);
    rgba += G<int>(1); aho_test::expect_rgba(rgba, 1, 3, 3, 4);
    rgba -= G<int>(1); aho_test::expect_rgba(rgba, 1, 2, 3, 4);
    rgba += B<int>(1); aho_test::expect_rgba(rgba, 1, 2, 4, 4);
    rgba -= B<int>(1); aho_test::expect_rgba(rgba, 1, 2, 3, 4);
    EXPECT_EQ(&(rgba += A<int>(2)), &rgba); EXPECT_EQ(rgba.a.value, 6);
    rgba -= A<int>(1); EXPECT_EQ(rgba.a.value, 5);
    HSV<int, int> hsv(1, 2, 3);
    hsv += H<int>(1); aho_test::expect_hsv(hsv, 2, 2, 3);
    hsv -= H<int>(1); aho_test::expect_hsv(hsv, 1, 2, 3);
    hsv += S<int>(1); aho_test::expect_hsv(hsv, 1, 3, 3);
    hsv -= S<int>(1); aho_test::expect_hsv(hsv, 1, 2, 3);
    hsv += V<int>(1); aho_test::expect_hsv(hsv, 1, 2, 4);
    hsv -= V<int>(1); aho_test::expect_hsv(hsv, 1, 2, 3);
    EXPECT_EQ(hsv.h.value, 1); EXPECT_EQ(hsv.s.value, 2); EXPECT_EQ(hsv.v.value, 3);
    HSVA<int, int> hsva(1, 2, 3, 4);
    hsva += H<int>(1); aho_test::expect_hsva(hsva, 2, 2, 3, 4);
    hsva -= H<int>(1); aho_test::expect_hsva(hsva, 1, 2, 3, 4);
    hsva += S<int>(1); aho_test::expect_hsva(hsva, 1, 3, 3, 4);
    hsva -= S<int>(1); aho_test::expect_hsva(hsva, 1, 2, 3, 4);
    hsva += V<int>(1); aho_test::expect_hsva(hsva, 1, 2, 4, 4);
    hsva -= V<int>(1); aho_test::expect_hsva(hsva, 1, 2, 3, 4);
    hsva += A<int>(1); aho_test::expect_hsva(hsva, 1, 2, 3, 5);
    hsva -= A<int>(1); aho_test::expect_hsva(hsva, 1, 2, 3, 4);
}
TEST(ColorLiteralTest, ChannelLiteralsPreserveValuesAndTypes) {
    using namespace aho::literals;
    EXPECT_EQ((2_r).value, 2); EXPECT_EQ((3_g).value, 3); EXPECT_EQ((4_b).value, 4);
    EXPECT_EQ((5_h).value, 5); EXPECT_EQ((6_s).value, 6); EXPECT_EQ((7_v).value, 7); EXPECT_EQ((8_a).value, 8);
    EXPECT_EQ((0.5_r).value, 0.5); EXPECT_EQ((0.5_f_g).value, 0.5f); EXPECT_EQ((2_l_b).value, 2u);
    EXPECT_EQ((0.5_g).value, 0.5); EXPECT_EQ((0.5_b).value, 0.5); EXPECT_EQ((0.5_h).value, 0.5);
    EXPECT_EQ((0.5_s).value, 0.5); EXPECT_EQ((0.5_v).value, 0.5); EXPECT_EQ((0.5_a).value, 0.5);
    EXPECT_EQ((0.5_f_r).value, 0.5f); EXPECT_EQ((0.5_f_b).value, 0.5f); EXPECT_EQ((0.5_f_h).value, 0.5f);
    EXPECT_EQ((0.5_f_s).value, 0.5f); EXPECT_EQ((0.5_f_v).value, 0.5f); EXPECT_EQ((0.5_f_a).value, 0.5f);
    EXPECT_EQ((2_l_r).value, 2u); EXPECT_EQ((2_l_g).value, 2u); EXPECT_EQ((2_l_h).value, 2u);
    EXPECT_EQ((2_l_s).value, 2u); EXPECT_EQ((2_l_v).value, 2u); EXPECT_EQ((2_l_a).value, 2u);
}
TEST(ColorCompositionTest, IndependentChannelsComposeWithoutChangingOtherChannels) {
    using namespace aho::color;
    const auto rgb = R<double>(2) + G<double>(3);
    EXPECT_EQ(rgb.r.value, 2); EXPECT_EQ(rgb.g.value, 3); EXPECT_EQ(rgb.b.value, 0);
    const auto rb = R<double>(2) + B<double>(4);
    EXPECT_EQ(rb.r.value, 2); EXPECT_EQ(rb.g.value, 0); EXPECT_EQ(rb.b.value, 4);
    const auto hsv = H<double>(120) + S<double>(0.5);
    EXPECT_EQ(hsv.h.value, 120); EXPECT_EQ(hsv.s.value, 0.5); EXPECT_EQ(hsv.v.value, 0);
    const auto rgba = A<double>(0.5) + R<double>(2);
    EXPECT_EQ(rgba.r.value, 2); EXPECT_EQ(rgba.a.value, 0.5);
    const auto hsva = A<double>(0.5) + H<double>(120);
    EXPECT_EQ(hsva.h.value, 120); EXPECT_EQ(hsva.a.value, 0.5);
    aho_test::expect_rgba(R<double>(2) + A<double>(0.5), 2, 0, 0, 0.5);
    aho_test::expect_hsva(H<double>(120) + A<double>(0.5), 120, 0, 0, 0.5);
}
TEST(ColorCompositionTest, ChannelSubtractionPreservesLeftOperandSign) {
    using namespace aho::color;
    const auto rgb = R<double>(2) - G<double>(3);
    EXPECT_EQ(rgb.r.value, 2); EXPECT_EQ(rgb.g.value, -3); EXPECT_EQ(rgb.b.value, 0);
    const auto hsv = H<double>(120) - S<double>(0.5);
    EXPECT_EQ(hsv.h.value, 120); EXPECT_EQ(hsv.s.value, -0.5);
    aho_test::expect_rgba(R<double>(2) - A<double>(0.5), 2, 0, 0, -0.5);
    aho_test::expect_rgba(A<double>(0.5) - R<double>(2), -2, 0, 0, 0.5);
    aho_test::expect_hsva(H<double>(120) - A<double>(0.5), 120, 0, 0, -0.5);
    aho_test::expect_hsva(A<double>(0.5) - H<double>(120), -120, 0, 0, 0.5);
}
TEST(ColorOutputTest, AllColorSpaceOutputsNameAndPreserveEveryChannel) {
    auto output = [](const auto& color) { std::ostringstream stream; stream << color; return stream.str(); };
    EXPECT_EQ(output(aho::color::RGB<int>(1, 2, 3)), "(r, g, b)=(1, 2, 3)");
    EXPECT_EQ(output(aho::color::RGBA<int>(1, 2, 3, 4)), "(r, g, b, a)=(1, 2, 3, 4)");
    EXPECT_EQ(output(aho::color::HSV<int, int>(1, 2, 3)), "(h, s, v)=(1, 2, 3)");
    EXPECT_EQ(output(aho::color::HSVA<int, int>(1, 2, 3, 4)), "(h, s, v, a)=(1, 2, 3, 4)");
}
}
#endif
