#include <AHO/core/math/projection.hpp>
#include <gtest/gtest.h>

namespace {
    template<typename T>
    class ProjectionPolicyTest : public testing::Test {};

    template<typename T>
    class OrthographicTest : public testing::Test {};

    template<typename T>
    class OrthographicPolicyTest : public testing::Test {};

    using FloatingTypes = testing::Types<float, double>;
    TYPED_TEST_SUITE(ProjectionPolicyTest, FloatingTypes);
    TYPED_TEST_SUITE(OrthographicTest, FloatingTypes);
    TYPED_TEST_SUITE(OrthographicPolicyTest, FloatingTypes);

    template<typename T, aho::HandedCoordinate handed, aho::DepthRange depth, aho::ClipY y, aho::VectorConvention convention>
    void expect_projection();

    template<typename T, aho::HandedCoordinate handed, aho::DepthRange depth, aho::ClipY y, aho::VectorConvention convention>
    void expect_orthographic();

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

namespace {
    TYPED_TEST(OrthographicTest, OffCenterBoundsMapCornersAndDepthToTheClipVolume) {
        using T = TypeParam;
        const auto matrix = aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{2}, T{10}, T{-3}, T{5}, T{1}, T{11});
        const auto near = aho::math::to_array(aho::matrix::project_point(matrix, aho::d3::Point<T>{2, -3, -1}));
        const auto far = aho::math::to_array(aho::matrix::project_point(matrix, aho::d3::Point<T>{10, 5, -11}));
        EXPECT_NEAR(near[0], -1, 1e-6);
        EXPECT_NEAR(near[1], -1, 1e-6);
        EXPECT_NEAR(near[2], 0, 1e-6);
        EXPECT_NEAR(far[0], 1, 1e-6);
        EXPECT_NEAR(far[1], 1, 1e-6);
        EXPECT_NEAR(far[2], 1, 1e-6);
        const auto inverse = aho::matrix::inverse_affine(matrix);
        ASSERT_TRUE(inverse);
        const auto world = aho::math::to_array(aho::matrix::project_point(*inverse, aho::d3::Point<T>{1, 1, 1}));
        EXPECT_NEAR(world[0], 10, 1e-5);
        EXPECT_NEAR(world[1], 5, 1e-5);
        EXPECT_NEAR(world[2], -11, 1e-5);
    }

    TYPED_TEST(OrthographicTest, VulkanYFlipIncludesOffCenterTranslationAndAllowsZeroNear) {
        using T = TypeParam;
        const auto matrix = aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(T{0}, T{800}, T{0}, T{600}, T{0}, T{100});
        const auto bottom = aho::math::to_array(aho::matrix::project_point(matrix, aho::d3::Point<T>{0, 0, 0}));
        const auto top = aho::math::to_array(aho::matrix::project_point(matrix, aho::d3::Point<T>{800, 600, -100}));
        EXPECT_NEAR(bottom[0], -1, 1e-6);
        EXPECT_NEAR(bottom[1], 1, 1e-6);
        EXPECT_NEAR(top[0], 1, 1e-6);
        EXPECT_NEAR(top[1], -1, 1e-6);
        EXPECT_NEAR(top[2], 1, 1e-6);
        const auto negative_near = aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{-1}, T{1}, T{-1}, T{1}, T{-10}, T{10});
        EXPECT_NEAR(aho::math::to_array(aho::matrix::project_point(negative_near, aho::d3::Point<T>{0, 0, 10}))[2], 0, 1e-6);
    }

    TYPED_TEST(OrthographicTest, LargeSymmetricBoundsKeepFiniteCoefficientsAndCorrectCorners) {
        using T = TypeParam;
        const auto maximum = std::numeric_limits<T>::max();
        const auto matrix = aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(-maximum, maximum, -maximum, maximum, -maximum, maximum);
        const auto corner = aho::math::to_array(aho::matrix::project_point(matrix, aho::d3::Point<T>{maximum, maximum, -maximum}));
        for (auto value : corner)
            EXPECT_NEAR(value, 1, 2e-6);
    }

    TYPED_TEST(OrthographicTest, DegenerateNonFiniteAndUnrepresentableBoundsAreRejected) {
        using T = TypeParam;
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, T{0}, T{0}, T{1}, T{0}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{1}, T{0}, T{0}, T{1}, T{0}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, T{1}, T{1}, T{1}, T{0}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, T{1}, T{1}, T{0}, T{0}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, T{1}, T{0}, T{1}, T{1}, T{0})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, T{1}, T{0}, T{1}, T{1}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, std::numeric_limits<T>::infinity(), T{0}, T{1}, T{0}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, T{1}, std::numeric_limits<T>::quiet_NaN(), T{1}, T{0}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(T{0}, std::numeric_limits<T>::denorm_min(), T{0}, T{1}, T{0}, T{1})), std::invalid_argument);
    }
} // namespace

namespace {
    template<typename T, aho::HandedCoordinate handed, aho::DepthRange depth, aho::ClipY y, aho::VectorConvention convention>
    void expect_orthographic() {
        const auto matrix = aho::matrix::make_orthographic<handed, depth, y, convention>(T{-2}, T{2}, T{-1}, T{1}, T{1}, T{11});
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

    TYPED_TEST(OrthographicPolicyTest, HandednessDepthYAndVectorPoliciesAreIndependent) {
        using T = TypeParam;
        using H = aho::HandedCoordinate;
        using D = aho::DepthRange;
        using Y = aho::ClipY;
        using C = aho::VectorConvention;
        expect_orthographic<T, H::LEFT, D::ZERO_TO_ONE, Y::UP, C::COLUMN>();
        expect_orthographic<T, H::LEFT, D::ZERO_TO_ONE, Y::UP, C::ROW>();
        expect_orthographic<T, H::LEFT, D::ZERO_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_orthographic<T, H::LEFT, D::ZERO_TO_ONE, Y::DOWN, C::ROW>();
        expect_orthographic<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::COLUMN>();
        expect_orthographic<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::ROW>();
        expect_orthographic<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_orthographic<T, H::LEFT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::ROW>();
        expect_orthographic<T, H::RIGHT, D::ZERO_TO_ONE, Y::UP, C::COLUMN>();
        expect_orthographic<T, H::RIGHT, D::ZERO_TO_ONE, Y::UP, C::ROW>();
        expect_orthographic<T, H::RIGHT, D::ZERO_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_orthographic<T, H::RIGHT, D::ZERO_TO_ONE, Y::DOWN, C::ROW>();
        expect_orthographic<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::COLUMN>();
        expect_orthographic<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::UP, C::ROW>();
        expect_orthographic<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::COLUMN>();
        expect_orthographic<T, H::RIGHT, D::NEGATIVE_ONE_TO_ONE, Y::DOWN, C::ROW>();
    }

} // namespace
