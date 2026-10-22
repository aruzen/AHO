#include <AHO/core/math/functions.hpp>
#include <gtest/gtest.h>
#include <limits>

namespace {
    TEST(MathFunctionsTest, ScalarLerpKeepsEndpointsAndAllowsExtrapolation) {
        static_assert(aho::math::lerp(2.0, 10.0, .25) == 4);
        EXPECT_FLOAT_EQ(aho::math::lerp(2.f, 10.f, 0.f), 2);
        EXPECT_FLOAT_EQ(aho::math::lerp(2.f, 10.f, 1.f), 10);
        EXPECT_DOUBLE_EQ(aho::math::lerp(2.0, 10.0, 2.0), 18);
        const auto maximum = std::numeric_limits<double>::max();
        EXPECT_DOUBLE_EQ(aho::math::lerp(-maximum, maximum, .5), 0);
    }

    TEST(MathFunctionsTest, CoordinateLerpPreservesPointVectorAndEnabledAxes) {
        const auto vector = aho::math::lerp(aho::d3::VectorF{0, 4, 8}, aho::d3::VectorF{8, 12, 16}, .25f);
        EXPECT_EQ(aho::math::to_array(vector), (std::array{2.f, 6.f, 10.f}));
        const auto point = aho::math::lerp(aho::d2::PointD{2, 4}, aho::d2::PointD{6, 12}, .5);
        static_assert(std::same_as<std::remove_cvref_t<decltype(point)>, aho::d2::PointD>);
        EXPECT_EQ(aho::math::to_array(point), (std::array{4.0, 8.0}));
        using XZ = aho::coordinate::MakeCoordinateInfo<true, false, true>::Value;
        using VectorXZ = aho::Vector_<float, vsl::D2, XZ>;
        EXPECT_EQ(aho::math::to_array(aho::math::lerp(VectorXZ{2, 6}, VectorXZ{6, 14}, .5f)), (std::array{4.f, 10.f}));
        EXPECT_EQ(aho::math::to_array(aho::math::lerp(aho::d1::VectorF{1}, aho::d1::VectorF{3}, .5f)), (std::array{2.f}));
    }

    TEST(MathFunctionsTest, ArrayLerpCoversRGBAAndHomogeneousDataWithoutA4DCoordinateType) {
        constexpr auto result = aho::math::lerp(std::array{0.f, 2.f, 4.f, 1.f}, std::array{4.f, 6.f, 8.f, 1.f}, .5f);
        static_assert(result == std::array{2.f, 4.f, 6.f, 1.f});
        EXPECT_EQ(result, (std::array{2.f, 4.f, 6.f, 1.f}));
    }

    TEST(MathFunctionsTest, SmoothstepClampsAndInterpolatesTheCubicCurve) {
        static_assert(aho::math::smoothstep(0.0, 1.0, .25) == .15625);
        EXPECT_FLOAT_EQ(aho::math::smoothstep(2.f, 6.f, -10.f), 0);
        EXPECT_FLOAT_EQ(aho::math::smoothstep(2.f, 6.f, 4.f), .5);
        EXPECT_FLOAT_EQ(aho::math::smoothstep(2.f, 6.f, 20.f), 1);
        const auto maximum = std::numeric_limits<double>::max();
        EXPECT_DOUBLE_EQ(aho::math::smoothstep(-maximum, maximum, 0.0), .5);
        EXPECT_DOUBLE_EQ(aho::math::smoothstep(0.0, 1.0, std::numeric_limits<double>::infinity()), 1);
    }

    TEST(MathFunctionsTest, SmoothstepRejectsEqualReversedAndNonFiniteEdgesAndNaN) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        const auto infinity = std::numeric_limits<float>::infinity();
        EXPECT_THROW(aho::math::smoothstep(1.f, 1.f, .5f), std::invalid_argument);
        EXPECT_THROW(aho::math::smoothstep(1.f, 0.f, .5f), std::invalid_argument);
        EXPECT_THROW(aho::math::smoothstep(-infinity, 1.f, .5f), std::invalid_argument);
        EXPECT_THROW(aho::math::smoothstep(0.f, infinity, .5f), std::invalid_argument);
        EXPECT_THROW(aho::math::smoothstep(0.f, 1.f, nan), std::invalid_argument);
    }

    TEST(MathFunctionsTest, DistanceWorksForPointsVectorsAndCustomCoordinates) {
        EXPECT_FLOAT_EQ(aho::math::distance(aho::d1::PointF{-2}, aho::d1::PointF{3}), 5);
        EXPECT_DOUBLE_EQ(aho::math::distance(aho::d2::PointD{1, 2}, aho::d2::PointD{4, 6}), 5);
        EXPECT_FLOAT_EQ(aho::math::distance(aho::d3::VectorF{1, 2, 3}, aho::d3::VectorF{3, 5, 9}), 7);
        using YZ = aho::coordinate::MakeCoordinateInfo<false, true, true>::Value;
        using PointYZ = aho::Point_<double, vsl::D2, YZ>;
        EXPECT_DOUBLE_EQ(aho::math::distance(PointYZ{2, 3}, PointYZ{5, 7}), 5);
        const auto large = std::numeric_limits<double>::max() / 4;
        EXPECT_TRUE(std::isfinite(aho::math::distance(aho::d2::VectorD{0, 0}, aho::d2::VectorD{large, large})));
    }

    TEST(MathFunctionsTest, CheckedNormalizationUsesExistingVectorArithmetic) {
        EXPECT_EQ(aho::math::to_array(aho::math::normalized(aho::d1::VectorF{-3})), (std::array{-1.f}));
        const auto vector = aho::math::normalized(aho::d2::VectorD{3, 4});
        EXPECT_NEAR(vector.value.___AN1, .6, 1e-14);
        EXPECT_NEAR(vector.value.___AN2, .8, 1e-14);
        EXPECT_EQ(aho::math::to_array(aho::math::normalized(aho::d3::VectorF{0, 0, 5})), (std::array{0.f, 0.f, 1.f}));
        EXPECT_THROW(aho::math::normalized(aho::d3::VectorF{0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(aho::math::normalized(aho::d2::VectorD{std::numeric_limits<double>::infinity(), 0}), std::invalid_argument);
        EXPECT_THROW(aho::math::normalized(aho::d3::VectorF{0, std::numeric_limits<float>::quiet_NaN(), 0}), std::invalid_argument);
    }
} // namespace
