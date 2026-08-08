#include <aho/core/color.hpp>

#include <gtest/gtest.h>

TEST(AHOColor, AddsRgbComponents)
{
    const aho::color::RGB<int> lhs{10, 20, 30};
    const aho::color::RGB<int> rhs{1, 2, 3};

    const auto result = lhs + rhs;

    EXPECT_EQ(result.r.value, 11);
    EXPECT_EQ(result.g.value, 22);
    EXPECT_EQ(result.b.value, 33);
}

TEST(AHOColor, KeepsAlphaWhenScalingRgba)
{
    const aho::color::RGBA<int> color{2, 4, 6, 8};

    const auto result = color * 3;

    EXPECT_EQ(result.r.value, 6);
    EXPECT_EQ(result.g.value, 12);
    EXPECT_EQ(result.b.value, 18);
    EXPECT_EQ(result.a.value, 8);
}

TEST(AHOColor, ConvertsPrimaryRedRgbToHsv)
{
    aho::color::RGB<float> red{1.0f, 0.0f, 0.0f};

    const auto hsv = red.hsv<float, float>();

    EXPECT_FLOAT_EQ(hsv.h.value, 0.0f);
    EXPECT_FLOAT_EQ(hsv.s.value, 1.0f);
    EXPECT_FLOAT_EQ(hsv.v.value, 1.0f);
}

TEST(AHOColor, ConvertsHsvToPrimaryBlueRgb)
{
    aho::color::HSV<float, float> blue{240.0f, 1.0f, 1.0f};

    const auto rgb = blue.rgb<float>();

    EXPECT_NEAR(rgb.r.value, 0.0f, 1.0e-6f);
    EXPECT_NEAR(rgb.g.value, 0.0f, 1.0e-6f);
    EXPECT_NEAR(rgb.b.value, 1.0f, 1.0e-6f);
}
