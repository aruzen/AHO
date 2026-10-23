#include <AHO/core/math/projection.hpp>
#include <gtest/gtest.h>

namespace {
    template<typename T>
    class ProjectionPolicyTest : public testing::Test {};
    using FloatingTypes = testing::Types<float, double>;
    TYPED_TEST_SUITE(ProjectionPolicyTest, FloatingTypes);

    template<typename T, aho::HandedCoordinate handed, aho::DepthRange depth, aho::ClipY y, aho::VectorConvention convention>
    void expect_projection();

    template<typename T, aho::HandedCoordinate handed, aho::DepthRange depth, aho::ClipY y, aho::VectorConvention convention>
    void expect_projection() {
        const auto matrix = aho::matrix::make_perspective<handed, depth, y, convention>(aho::Degree{90}, T{2}, T{1}, T{11});
        const T sign = handed == aho::HandedCoordinate::LEFT ? T{1} : T{-1};
        const auto near = aho::math::to_array(aho::matrix::project_point<convention>(matrix, aho::d3::Point<T>{0, 0, sign}));
        const auto far = aho::math::to_array(aho::matrix::project_point<convention>(matrix, aho::d3::Point<T>{0, 0, sign * 11}));
        EXPECT_NEAR(near[2], depth == aho::DepthRange::ZERO_TO_ONE ? 0 : -1, 1e-6);
        EXPECT_NEAR(far[2], 1, 1e-6);
        const auto corner = aho::math::to_array(aho::matrix::project_point<convention>(matrix, aho::d3::Point<T>{2, 1, sign}));
        EXPECT_NEAR(corner[0], 1, 1e-6);
        EXPECT_NEAR(corner[1], y == aho::ClipY::UP ? 1 : -1, 1e-6);
        const auto clip = aho::matrix::transform_homogeneous<convention>(matrix, std::array<T, 4>{0, 0, sign, 1});
        EXPECT_GT(clip[3], 0);
    }

    TYPED_TEST(ProjectionPolicyTest, HandednessDepthYAndVectorPoliciesAreIndependent) {
        using T = TypeParam;
        using H = aho::HandedCoordinate;
        using D = aho::DepthRange;
        using Y = aho::ClipY;
        using C = aho::VectorConvention;
        expect_projection<T, H::LEFT, D::ZERO_TO_ONE, Y::UP, C::COLUMN>();
        expect_projection<T, H::LEFT, D::ZERO_TO_ONE, Y::UP, C::ROW>();
        expect_projection<T, H::LEFT, D::ZERO_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_projection<T, H::LEFT, D::ZERO_TO_ONE, Y::DOWN, C::ROW>();
        expect_projection<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::COLUMN>();
        expect_projection<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::ROW>();
        expect_projection<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_projection<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::ROW>();
        expect_projection<T, H::RIGHT, D::ZERO_TO_ONE, Y::UP, C::COLUMN>();
        expect_projection<T, H::RIGHT, D::ZERO_TO_ONE, Y::UP, C::ROW>();
        expect_projection<T, H::RIGHT, D::ZERO_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_projection<T, H::RIGHT, D::ZERO_TO_ONE, Y::DOWN, C::ROW>();
        expect_projection<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::COLUMN>();
        expect_projection<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::ROW>();
        expect_projection<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_projection<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::ROW>();
    }

    TEST(ProjectionDefaultsTest, DefaultCoordinateSystemIsLeftHanded) {
        static_assert(aho::DEFAULT_COORDINATE_SYSTEM == aho::HandedCoordinate::LEFT);
        const auto matrix = aho::matrix::make_perspective(aho::Degree{90}, 1.f, 1.f, 10.f);
        EXPECT_GT(matrix[3][2], 0);
        EXPECT_NEAR(aho::math::to_array(aho::matrix::project_point(matrix, aho::d3::PointF{0, 0, 1}))[2], 0, 1e-6);
    }
} // namespace
