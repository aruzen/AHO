#pragma once
#include <gtest/gtest.h>

namespace aho_test {
    // These arithmetic cases use small integers and binary fractions, all exactly representable.
    template<class Color>
    void expect_rgb(const Color& color, double r, double g, double b) {
        EXPECT_DOUBLE_EQ(color.r.value, r);
        EXPECT_DOUBLE_EQ(color.g.value, g);
        EXPECT_DOUBLE_EQ(color.b.value, b);
    }
    template<class Color>
    void expect_rgba(const Color& color, double r, double g, double b, double a) {
        expect_rgb(color, r, g, b);
        EXPECT_DOUBLE_EQ(color.a.value, a);
    }
    template<class Color>
    void expect_hsv(const Color& color, double h, double s, double v) {
        EXPECT_DOUBLE_EQ(color.h.value, h);
        EXPECT_DOUBLE_EQ(color.s.value, s);
        EXPECT_DOUBLE_EQ(color.v.value, v);
    }
    template<class Color>
    void expect_hsva(const Color& color, double h, double s, double v, double a) {
        expect_hsv(color, h, s, v);
        EXPECT_DOUBLE_EQ(color.a.value, a);
    }
} // namespace aho_test
