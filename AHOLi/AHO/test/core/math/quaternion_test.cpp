#include <AHO/core/math/quaternion.hpp>
#include <gtest/gtest.h>
#include <limits>

namespace {
    template<typename T>
    class QuaternionTest : public testing::Test {};
    using FloatingTypes = testing::Types<float, double>;
    TYPED_TEST_SUITE(QuaternionTest, FloatingTypes);

    TEST(QuaternionConstexprTest, IdentityInverseComponentsAndRotationAreConstantEvaluable) {
        constexpr aho::QuaternionF identity;
        static_assert(identity.components() == std::array{0.f, 0.f, 0.f, 1.f});
        static_assert(identity.inverse().components()[3] == 1);
        static_assert(identity.to_matrix()[2][2] == 1);
        constexpr auto rotated = identity.rotate(aho::d3::VectorF{1, 2, 3});
        static_assert(aho::math::to_array(rotated) == std::array{1.f, 2.f, 3.f});
        EXPECT_EQ(identity.components(), (std::array{0.f, 0.f, 0.f, 1.f}));
    }

    TYPED_TEST(QuaternionTest, ComponentsAreNormalizedWithoutOverflowOrUnderflow) {
        using T = TypeParam;
        using Q = aho::Quaternion<T, aho::HandedCoordinate::RIGHT>;
        for (auto scale : {T{1}, std::numeric_limits<T>::max(), std::numeric_limits<T>::denorm_min()}) {
            const auto components = Q::FromComponents(scale, scale, scale, scale).components();
            for (auto value : components)
                EXPECT_NEAR(value, .5, 1e-6);
        }
        const auto gltf = Q::FromComponents(0, 0, 3, 4).components();
        EXPECT_EQ(gltf[0], 0);
        EXPECT_EQ(gltf[1], 0);
        EXPECT_NEAR(gltf[2], .6, 1e-6);
        EXPECT_NEAR(gltf[3], .8, 1e-6);
        EXPECT_THROW(Q::FromComponents(0, 0, 0, 0), std::invalid_argument);
        EXPECT_THROW(Q::FromComponents(0, 0, std::numeric_limits<T>::infinity(), 1), std::invalid_argument);
        EXPECT_THROW(Q::FromComponents(std::numeric_limits<T>::quiet_NaN(), 0, 0, 1), std::invalid_argument);
    }

    TYPED_TEST(QuaternionTest, AxisAngleRotatesRightHandedAndAgreesWithTheExistingMatrixFactory) {
        using T = TypeParam;
        using Q = aho::Quaternion<T, aho::HandedCoordinate::RIGHT>;
        const auto rotation = Q::FromAxisAngle(aho::Degree{90}, aho::d3::Vector<T>{0, 0, 5});
        const auto radians = Q::FromAxisAngle(aho::Radian{std::numbers::pi_v<long double> / 2}, aho::d3::Vector<T>{0, 0, 1});
        EXPECT_EQ(rotation.components(), radians.components());
        const auto vector = aho::math::to_array(rotation.rotate(aho::d3::Vector<T>{1, 0, 0}));
        EXPECT_NEAR(vector[0], 0, 1e-6);
        EXPECT_NEAR(vector[1], 1, 1e-6);
        EXPECT_NEAR(vector[2], 0, 1e-6);
        const auto matrix = rotation.to_matrix();
        const auto expected = aho::matrix::make_rotation<aho::HandedCoordinate::RIGHT, aho::VectorConvention::COLUMN, T>(aho::Degree{90}, aho::d3::Vector<T>{0, 0, 1});
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                EXPECT_NEAR(matrix[r][c], expected[r][c], 1e-6);
        const auto maximum_axis = Q::FromAxisAngle(aho::Degree{90}, aho::d3::Vector<T>{0, 0, std::numeric_limits<T>::max()});
        EXPECT_EQ(maximum_axis.components(), rotation.components());
        const auto tiny_axis = Q::FromAxisAngle(aho::Degree{90}, aho::d3::Vector<T>{0, 0, std::numeric_limits<T>::denorm_min()});
        EXPECT_EQ(tiny_axis.components(), rotation.components());
        EXPECT_THROW(Q::FromAxisAngle(aho::Degree{0}, aho::d3::Vector<T>{0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(Q::FromAxisAngle(aho::Radian{1}, aho::d3::Vector<T>{0, std::numeric_limits<T>::infinity(), 0}), std::invalid_argument);
        EXPECT_THROW(Q::FromAxisAngle(aho::Radian{std::numeric_limits<long double>::quiet_NaN()}, aho::d3::Vector<T>{0, 1, 0}), std::invalid_argument);
    }

    TYPED_TEST(QuaternionTest, CompositionOrderMatchesMatricesAndInverseRestoresTheOriginalVector) {
        using T = TypeParam;
        using Q = aho::Quaternion<T, aho::HandedCoordinate::RIGHT>;
        const auto x = Q::FromAxisAngle(aho::Degree{90}, aho::d3::Vector<T>{1, 0, 0});
        const auto y = Q::FromAxisAngle(aho::Degree{90}, aho::d3::Vector<T>{0, 1, 0});
        const auto combined = x * y;
        const aho::d3::Vector<T> forward{0, 0, 1};
        const auto rotated = aho::math::to_array(combined.rotate(forward));
        EXPECT_NEAR(rotated[0], 1, 1e-6);
        EXPECT_NEAR(rotated[1], 0, 1e-6);
        EXPECT_NEAR(rotated[2], 0, 1e-6);
        const auto reversed = aho::math::to_array((y * x).rotate(forward));
        EXPECT_NEAR(reversed[0], 0, 1e-6);
        EXPECT_NEAR(reversed[1], -1, 1e-6);
        const auto restored = aho::math::to_array(combined.inverse().rotate(combined.rotate(forward)));
        EXPECT_NEAR(restored[0], 0, 1e-6);
        EXPECT_NEAR(restored[1], 0, 1e-6);
        EXPECT_NEAR(restored[2], 1, 1e-6);
        const auto actual = combined.to_matrix(), expected = x.to_matrix() * y.to_matrix();
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                EXPECT_NEAR(actual[r][c], expected[r][c], 1e-6);
    }

    TYPED_TEST(QuaternionTest, RepeatedCompositionPreservesUnitLengthAndVectorLength) {
        using T = TypeParam;
        using Q = aho::Quaternion<T, aho::HandedCoordinate::RIGHT>;
        const auto step = Q::FromAxisAngle(aho::Degree{.1}, aho::d3::Vector<T>{1, 2, 3});
        Q rotation;
        for (int i = 0; i < 10000; ++i)
            rotation = step * rotation;
        long double length = 0;
        for (auto component : rotation.components())
            length += static_cast<long double>(component) * component;
        EXPECT_NEAR(length, 1, 1e-6);
        EXPECT_NEAR(rotation.rotate(aho::d3::Vector<T>{3, 4, 0}).length(), 5, 1e-5);
    }

    TYPED_TEST(QuaternionTest, SlerpKeepsEndpointsInterpolatesAngularMotionAndAllowsExtrapolation) {
        using T = TypeParam;
        using Q = aho::Quaternion<T, aho::HandedCoordinate::RIGHT>;
        const Q first;
        const auto second = Q::FromAxisAngle(aho::Degree{90}, aho::d3::Vector<T>{0, 0, 1});
        EXPECT_EQ(aho::math::slerp(first, second, T{0}).components(), first.components());
        EXPECT_EQ(aho::math::slerp(first, second, T{1}).components(), second.components());
        const auto middle = aho::math::to_array(aho::math::slerp(first, second, T{.5}).rotate(aho::d3::Vector<T>{1, 0, 0}));
        EXPECT_NEAR(middle[0], std::sqrt(.5), 1e-6);
        EXPECT_NEAR(middle[1], std::sqrt(.5), 1e-6);
        const auto extrapolated = aho::math::to_array(aho::math::slerp(first, second, T{2}).rotate(aho::d3::Vector<T>{1, 0, 0}));
        EXPECT_NEAR(extrapolated[0], -1, 1e-6);
        EXPECT_NEAR(extrapolated[1], 0, 1e-6);
        EXPECT_THROW(aho::math::slerp(first, second, std::numeric_limits<T>::quiet_NaN()), std::invalid_argument);
        EXPECT_THROW(aho::math::slerp(first, second, std::numeric_limits<T>::infinity()), std::invalid_argument);
    }

    TYPED_TEST(QuaternionTest, SlerpTakesShortestArcAndHandlesEqualAndOppositeRepresentations) {
        using T = TypeParam;
        using Q = aho::Quaternion<T, aho::HandedCoordinate::RIGHT>;
        const auto first = Q::FromAxisAngle(aho::Degree{170}, aho::d3::Vector<T>{0, 0, 1});
        const auto second = Q::FromAxisAngle(aho::Degree{-170}, aho::d3::Vector<T>{0, 0, 1});
        const auto middle = aho::math::to_array(aho::math::slerp(first, second, T{.5}).rotate(aho::d3::Vector<T>{1, 0, 0}));
        EXPECT_NEAR(middle[0], -1, 1e-6);
        EXPECT_NEAR(middle[1], 0, 1e-6);
        const auto a = first.components();
        const auto opposite = Q::FromComponents(-a[0], -a[1], -a[2], -a[3]);
        const auto same_rotation = aho::math::slerp(first, opposite, T{.5}).to_matrix();
        const auto original = first.to_matrix();
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                EXPECT_NEAR(same_rotation[r][c], original[r][c], 1e-6);
        const auto identical = aho::math::slerp(first, first, T{.25}).components();
        for (std::size_t i = 0; i < 4; ++i)
            EXPECT_NEAR(identical[i], a[i], 1e-6);
        const auto tiny = Q::FromAxisAngle(aho::Degree{1e-8}, aho::d3::Vector<T>{0, 1, 0});
        for (auto value : aho::math::slerp(Q{}, tiny, T{.5}).components())
            EXPECT_TRUE(std::isfinite(value));
    }

    TYPED_TEST(QuaternionTest, TRSHandlesNonUniformNegativeAndZeroScaling) {
        using T = TypeParam;
        const auto rotation = aho::Quaternion<T, aho::HandedCoordinate::RIGHT>::FromAxisAngle(aho::Degree{90}, aho::d3::Vector<T>{0, 0, 1});
        const auto matrix = aho::matrix::make_trs(aho::d3::Vector<T>{10, 20, 30}, rotation, aho::d3::Vector<T>{2, -3, 4});
        const auto point = aho::math::to_array(aho::matrix::transform_point(matrix, aho::d3::Point<T>{1, 1, 1}));
        EXPECT_NEAR(point[0], 13, 1e-5);
        EXPECT_NEAR(point[1], 22, 1e-5);
        EXPECT_NEAR(point[2], 34, 1e-5);
        const auto inverse = aho::matrix::inverse_affine(matrix);
        ASSERT_TRUE(inverse);
        const auto original = aho::math::to_array(aho::matrix::transform_point(*inverse, aho::matrix::transform_point(matrix, aho::d3::Point<T>{1, 1, 1})));
        for (auto component : original)
            EXPECT_NEAR(component, 1, 1e-5);
        const auto collapsed = aho::matrix::make_trs(aho::d3::Vector<T>{10, 20, 30}, rotation, aho::d3::Vector<T>{0, 0, 0});
        EXPECT_EQ(aho::math::to_array(aho::matrix::transform_point(collapsed, aho::d3::Point<T>{1, 1, 1})), (std::array<T, 3>{10, 20, 30}));
        EXPECT_THROW(aho::matrix::make_trs(aho::d3::Vector<T>{std::numeric_limits<T>::infinity(), 0, 0}, rotation, aho::d3::Vector<T>{1, 1, 1}), std::invalid_argument);
        EXPECT_THROW(aho::matrix::make_trs(aho::d3::Vector<T>{0, 0, 0}, rotation, aho::d3::Vector<T>{1, std::numeric_limits<T>::quiet_NaN(), 1}), std::invalid_argument);
    }
} // namespace

namespace {
    template<typename Left, typename Right>
    concept composable_rotations = requires(Left left, Right right) { left * right; };

    TEST(QuaternionPolicyTest, HandednessIsPartOfTheTypeAndMatchesMatrixRotation) {
        using H = aho::HandedCoordinate;
        using C = aho::VectorConvention;
        using Left = aho::Quaternion<float, H::LEFT>;
        using Right = aho::Quaternion<float, H::RIGHT>;
        static_assert(std::same_as<aho::QuaternionF, Left>);
        static_assert(!composable_rotations<Left, Right>);
        const auto left = Left::FromAxisAngle(aho::Degree{90}, aho::d3::VectorF{0, 0, 1});
        const auto right = Right::FromAxisAngle(aho::Degree{90}, aho::d3::VectorF{0, 0, 1});
        EXPECT_NEAR(aho::math::to_array(left.rotate(aho::d3::VectorF{1, 0, 0}))[1], -1, 1e-6);
        EXPECT_NEAR(aho::math::to_array(right.rotate(aho::d3::VectorF{1, 0, 0}))[1], 1, 1e-6);
        const auto expected = aho::matrix::make_rotation<H::LEFT>(aho::Degree{90}, aho::d3::VectorF{0, 0, 1});
        const auto actual = left.to_matrix();
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                EXPECT_NEAR(actual[r][c], expected[r][c], 1e-6);
        EXPECT_EQ(left.to_matrix<C::ROW>(), aho::matrix::transpose(actual));
        const auto row = aho::matrix::make_trs<C::ROW>(aho::d3::VectorF{1, 2, 3}, left, aho::d3::VectorF{2, -3, 4});
        const auto column = aho::matrix::make_trs<C::COLUMN>(aho::d3::VectorF{1, 2, 3}, left, aho::d3::VectorF{2, -3, 4});
        EXPECT_EQ(row, aho::matrix::transpose(column));
    }

    TEST(QuaternionPolicyTest, DirectAndShortestInterpolationChooseDifferentPathsExplicitly) {
        using Q = aho::Quaternion<float, aho::HandedCoordinate::RIGHT>;
        const auto first = Q::FromAxisAngle(aho::Degree{170}, aho::d3::VectorF{0, 0, 1});
        const auto second = Q::FromAxisAngle(aho::Degree{-170}, aho::d3::VectorF{0, 0, 1});
        const auto direct = aho::math::slerp<aho::InterpolationPath::DIRECT>(first, second, .5f);
        const auto shortest = aho::math::slerp<aho::InterpolationPath::SHORTEST>(first, second, .5f);
        EXPECT_NEAR(aho::math::to_array(direct.rotate(aho::d3::VectorF{1, 0, 0}))[0], 1, 1e-6);
        EXPECT_NEAR(aho::math::to_array(shortest.rotate(aho::d3::VectorF{1, 0, 0}))[0], -1, 1e-6);
        const Q identity;
        const auto antipodal = Q::FromComponents(0, 0, 0, -1);
        EXPECT_THROW(aho::math::slerp<aho::InterpolationPath::DIRECT>(identity, antipodal, .5f), std::invalid_argument);
        EXPECT_EQ(aho::math::slerp<aho::InterpolationPath::DIRECT>(identity, antipodal, 0.f).components(), identity.components());
        EXPECT_EQ(aho::math::slerp<aho::InterpolationPath::DIRECT>(identity, antipodal, 1.f).components(), antipodal.components());
    }
} // namespace
