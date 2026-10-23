#include <AHO/core/math/projection.hpp>
#include <gtest/gtest.h>
#include <limits>

namespace {
    template<typename T>
    class MathTransformTest : public testing::Test {};
    using FloatingTypes = testing::Types<float, double>;
    TYPED_TEST_SUITE(MathTransformTest, FloatingTypes);

    TEST(MathTransferTest, RectangularMatricesHaveExplicitColumnMajorStorageAndRoundTrip) {
        constexpr aho::Mat<int, 2, 3> matrix{{1, 2, 3}, {4, 5, 6}};
        constexpr auto packed = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(matrix);
        static_assert(packed == std::array{1, 4, 2, 5, 3, 6});
        constexpr auto restored = aho::matrix::from_array<int, 2, 3, aho::MatrixLayout::COLUMN_MAJOR>(packed);
        static_assert(restored[0][2] == 3 && restored[1][0] == 4);
        constexpr auto transposed = aho::matrix::transpose(matrix);
        static_assert(transposed[2][0] == 3 && transposed[0][1] == 4);
        EXPECT_EQ(aho::matrix::transpose(transposed), matrix);
    }

    TYPED_TEST(MathTransformTest, TRSCompositionTransformsPointsAndDirectionsInTheDocumentedOrder) {
        using T = TypeParam;
        auto matrix = aho::matrix::translate(aho::matrix::make_identity<T, 4>(), aho::d3::Vector<T>{10, 20, 30});
        matrix = aho::matrix::rotate<aho::HandedCoordinate::RIGHT>(matrix, aho::Degree{90}, aho::d3::Vector<T>{0, 0, 2});
        matrix = aho::matrix::scale(matrix, aho::d3::Vector<T>{2, 3, 4});
        const auto point = aho::math::to_array(aho::matrix::transform_point(matrix, aho::d3::Point<T>{1, 0, 0}));
        const auto direction = aho::math::to_array(aho::matrix::transform_vector(matrix, aho::d3::Vector<T>{1, 0, 0}));
        EXPECT_NEAR(point[0], 10, 1e-5);
        EXPECT_NEAR(point[1], 22, 1e-5);
        EXPECT_NEAR(point[2], 30, 1e-5);
        EXPECT_NEAR(direction[0], 0, 1e-5);
        EXPECT_NEAR(direction[1], 2, 1e-5);
        EXPECT_NEAR(direction[2], 0, 1e-5);
        const auto packed = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(matrix);
        EXPECT_EQ(packed[12], 10);
        EXPECT_EQ(packed[13], 20);
        EXPECT_EQ(packed[14], 30);
        const auto inverse = aho::matrix::inverse_affine(matrix);
        ASSERT_TRUE(inverse);
        const auto original = aho::math::to_array(aho::matrix::transform_point(*inverse, aho::matrix::transform_point(matrix, aho::d3::Point<T>{1, 0, 0})));
        EXPECT_NEAR(original[0], 1, 1e-5);
        EXPECT_NEAR(original[1], 0, 1e-5);
        EXPECT_NEAR(original[2], 0, 1e-5);
    }

    TYPED_TEST(MathTransformTest, RotationRejectsZeroNonFiniteAxisAndAngle) {
        using T = TypeParam;
        const auto identity = aho::matrix::make_identity<T, 4>();
        EXPECT_THROW(aho::matrix::rotate<aho::HandedCoordinate::RIGHT>(identity, aho::Radian{1}, aho::d3::Vector<T>{0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(aho::matrix::rotate<aho::HandedCoordinate::RIGHT>(identity, aho::Degree{45}, aho::d3::Vector<T>{std::numeric_limits<T>::infinity(), 0, 0}), std::invalid_argument);
        EXPECT_THROW(aho::matrix::rotate<aho::HandedCoordinate::RIGHT>(identity, aho::Radian{std::numeric_limits<long double>::quiet_NaN()}, aho::d3::Vector<T>{0, 1, 0}), std::invalid_argument);
    }

    TYPED_TEST(MathTransformTest, InverseTransposeKeepsNormalsPerpendicularUnderNonUniformScale) {
        using T = TypeParam;
        auto matrix = aho::matrix::make_scaling(aho::d3::Vector<T>{2, 3, 4});
        matrix = aho::matrix::translate(matrix, aho::d3::Vector<T>{5, 6, 7});
        const auto normal = aho::matrix::transform_normal(matrix, aho::d3::Vector<T>{1, 1, 0});
        ASSERT_TRUE(normal);
        const auto tangent = aho::matrix::transform_vector(matrix, aho::d3::Vector<T>{1, -1, 0});
        EXPECT_NEAR(normal->dot(tangent), 0, 1e-6);
        const auto components = aho::math::to_array(*normal);
        EXPECT_NEAR(components[0], .5, 1e-6);
        EXPECT_NEAR(components[1], 1.0 / 3, 1e-6);
        const auto singular = aho::matrix::transform_normal(aho::matrix::make_scaling(aho::d3::Vector<T>{1, 0, 1}), aho::d3::Vector<T>{0, 1, 0});
        ASSERT_FALSE(singular);
        EXPECT_EQ(singular.error(), aho::matrix::FailedCause::SINGULAR);
        const auto projection = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{60}, T{1}, T{1}, T{100});
        const auto non_affine = aho::matrix::transform_normal(projection, aho::d3::Vector<T>{0, 1, 0});
        ASSERT_FALSE(non_affine);
        EXPECT_EQ(non_affine.error(), aho::matrix::FailedCause::NON_AFFINE);
    }

    TYPED_TEST(MathTransformTest, LookAtCentersTargetAndMovesEyeToOrigin) {
        using T = TypeParam;
        const aho::d3::Point<T> eye{10, 5, 8}, target{12, 6, 5};
        const auto view = aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(eye, target, aho::d3::Vector<T>{0, 1, 0});
        const auto origin = aho::math::to_array(aho::matrix::transform_point(view, eye));
        for (auto coordinate : origin)
            EXPECT_NEAR(coordinate, 0, 1e-5);
        const auto centered = aho::math::to_array(aho::matrix::transform_point(view, target));
        EXPECT_NEAR(centered[0], 0, 1e-5);
        EXPECT_NEAR(centered[1], 0, 1e-5);
        EXPECT_NEAR(centered[2], -std::sqrt(14.0), 1e-5);
        const auto projection = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(aho::Degree{66}, T{1}, T{.1}, T{1600});
        const auto ndc = aho::math::to_array(aho::matrix::project_point(projection * view, target));
        EXPECT_NEAR(ndc[0], 0, 1e-5);
        EXPECT_NEAR(ndc[1], 0, 1e-5);
        EXPECT_GT(ndc[2], 0);
        EXPECT_LT(ndc[2], 1);
    }

    TYPED_TEST(MathTransformTest, PerspectiveMapsDepthAndVulkanScreenDirections) {
        using T = TypeParam;
        const auto projection = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(aho::Degree{90}, T{2}, T{1}, T{11});
        const auto radians = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(aho::Radian{std::numbers::pi_v<long double> / 2}, T{2}, T{1}, T{11});
        EXPECT_EQ(projection, radians);
        const auto near = aho::math::to_array(aho::matrix::project_point(projection, aho::d3::Point<T>{0, 0, -1}));
        const auto far = aho::math::to_array(aho::matrix::project_point(projection, aho::d3::Point<T>{0, 0, -11}));
        EXPECT_NEAR(near[2], 0, 1e-6);
        EXPECT_NEAR(far[2], 1, 1e-6);
        const auto upper_right = aho::math::to_array(aho::matrix::project_point(projection, aho::d3::Point<T>{2, 1, -1}));
        EXPECT_NEAR(upper_right[0], 1, 1e-6);
        EXPECT_NEAR(upper_right[1], -1, 1e-6);
        const auto normal_y = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{90}, T{2}, T{1}, T{11});
        EXPECT_GT(normal_y[1][1], 0);
        const auto large = std::numeric_limits<T>::max() / 4;
        const auto wide = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{90}, T{1}, large, large * 2);
        EXPECT_TRUE(std::isfinite(wide[2][3]));
    }

    TYPED_TEST(MathTransformTest, CameraRejectsDegenerateViewAndProjectionInputs) {
        using T = TypeParam;
        const aho::d3::Point<T> eye{0, 0, 0}, target{0, 0, -1};
        const aho::d3::Vector<T> up{0, 1, 0};
        EXPECT_THROW(aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(eye, eye, up), std::invalid_argument);
        EXPECT_THROW(aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(eye, target, aho::d3::Vector<T>{0, 0, 1}), std::invalid_argument);
        EXPECT_THROW(aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(eye, target, aho::d3::Vector<T>{0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(aho::d3::Point<T>{std::numeric_limits<T>::infinity(), 0, 0}, target, up), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{0}, T{1}, T{1}, T{10})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{180}, T{1}, T{1}, T{10})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{60}, T{0}, T{1}, T{10})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{60}, T{1}, T{0}, T{10})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{60}, T{1}, T{10}, T{1})), std::invalid_argument);
        EXPECT_THROW((aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{60}, T{1}, T{1}, std::numeric_limits<T>::infinity())), std::invalid_argument);
        EXPECT_THROW((aho::matrix::project_point(aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::UP>(aho::Degree{60}, T{1}, T{1}, T{10}), eye)), std::invalid_argument);
    }

    TEST(MathTransferTest, PackedTransformMatchesShaderColumnMajorMultiplication) {
        auto matrix = aho::matrix::translate(aho::matrix::make_identity<float, 4>(), aho::d3::VectorF{4, 5, 6});
        matrix = aho::matrix::scale(matrix, aho::d3::VectorF{2, 3, 4});
        const auto packed = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(matrix);
        const std::array point{1.f, 2.f, 3.f, 1.f};
        std::array<float, 4> shader_result{};
        for (std::size_t c = 0; c < 4; ++c)
            for (std::size_t r = 0; r < 4; ++r)
                shader_result[r] += packed[c * 4 + r] * point[c];
        EXPECT_EQ(shader_result, (std::array{6.f, 11.f, 18.f, 1.f}));
        EXPECT_EQ(shader_result, aho::matrix::transform_homogeneous(matrix, point));
    }
} // namespace

namespace {
    template<typename T>
    class TransformPolicyTest : public testing::Test {};
    using PolicyScalars = testing::Types<float, double>;
    TYPED_TEST_SUITE(TransformPolicyTest, PolicyScalars);

    template<typename T, aho::HandedCoordinate handed, aho::ViewOrientation orientation, aho::VectorConvention convention>
    void expect_view();

    template<typename T, aho::HandedCoordinate handed, aho::ViewOrientation orientation, aho::VectorConvention convention>
    void expect_view() {
        const T front = handed == aho::HandedCoordinate::LEFT ? T{1} : T{-1};
        const T screen = orientation == aho::ViewOrientation::UPRIGHT ? T{1} : T{-1};
        const aho::d3::Point<T> eye{2, 3, 4};
        const auto view = aho::matrix::make_view<handed, orientation, convention>(eye, eye + aho::d3::Vector<T>{0, 0, front}, aho::d3::Vector<T>{0, 1, 0});
        const auto origin = aho::math::to_array(aho::matrix::transform_point<convention>(view, eye));
        EXPECT_EQ(origin, (std::array<T, 3>{0, 0, 0}));
        const auto target = aho::math::to_array(aho::matrix::transform_point<convention>(view, eye + aho::d3::Vector<T>{1, 1, front}));
        EXPECT_EQ(target, (std::array<T, 3>{screen, screen, front}));
    }

    TYPED_TEST(TransformPolicyTest, ViewHandednessRollAndVectorConventionAreIndependent) {
        using T = TypeParam;
        using H = aho::HandedCoordinate;
        using O = aho::ViewOrientation;
        using C = aho::VectorConvention;
        expect_view<T, H::LEFT, O::UPRIGHT, C::COLUMN>();
        expect_view<T, H::LEFT, O::UPRIGHT, C::ROW>();
        expect_view<T, H::LEFT, O::ROLL_180, C::COLUMN>();
        expect_view<T, H::LEFT, O::ROLL_180, C::ROW>();
        expect_view<T, H::RIGHT, O::UPRIGHT, C::COLUMN>();
        expect_view<T, H::RIGHT, O::UPRIGHT, C::ROW>();
        expect_view<T, H::RIGHT, O::ROLL_180, C::COLUMN>();
        expect_view<T, H::RIGHT, O::ROLL_180, C::ROW>();
    }

    TYPED_TEST(TransformPolicyTest, RowAndColumnCompositionRepresentTheSameWorldTransform) {
        using T = TypeParam;
        using C = aho::VectorConvention;
        using H = aho::HandedCoordinate;
        auto column = aho::matrix::translate<C::COLUMN>(aho::matrix::make_identity<T, 4>(), aho::d3::Vector<T>{10, 20, 30});
        column = aho::matrix::rotate<H::LEFT, C::COLUMN>(column, aho::Degree{90}, aho::d3::Vector<T>{0, 0, 1});
        column = aho::matrix::scale<C::COLUMN>(column, aho::d3::Vector<T>{2, 3, 4});
        auto row = aho::matrix::translate<C::ROW>(aho::matrix::make_identity<T, 4>(), aho::d3::Vector<T>{10, 20, 30});
        row = aho::matrix::rotate<H::LEFT, C::ROW>(row, aho::Degree{90}, aho::d3::Vector<T>{0, 0, 1});
        row = aho::matrix::scale<C::ROW>(row, aho::d3::Vector<T>{2, 3, 4});
        EXPECT_EQ(row, aho::matrix::transpose(column));
        const auto world = aho::math::to_array(aho::matrix::transform_point<C::ROW>(row, aho::d3::Point<T>{1, 0, 0}));
        EXPECT_NEAR(world[0], 10, 1e-5);
        EXPECT_NEAR(world[1], 18, 1e-5);
        EXPECT_NEAR(world[2], 30, 1e-5);
        EXPECT_EQ(aho::math::to_array(aho::matrix::transform_vector<C::ROW>(row, aho::d3::Vector<T>{1, 0, 0})),
                  aho::math::to_array(aho::matrix::transform_vector<C::COLUMN>(column, aho::d3::Vector<T>{1, 0, 0})));
        const auto a = aho::matrix::transform_normal<C::ROW>(row, aho::d3::Vector<T>{1, 1, 0});
        const auto b = aho::matrix::transform_normal<C::COLUMN>(column, aho::d3::Vector<T>{1, 1, 0});
        ASSERT_TRUE(a);
        ASSERT_TRUE(b);
        EXPECT_EQ(aho::math::to_array(*a), aho::math::to_array(*b));
    }

    TEST(TransformDefaultsTest, DefaultRotationAndViewUseLeftHandedCoordinates) {
        const auto rotation = aho::matrix::make_rotation(aho::Degree{90}, aho::d3::VectorF{0, 0, 1});
        EXPECT_NEAR(aho::math::to_array(aho::matrix::transform_vector(rotation, aho::d3::VectorF{1, 0, 0}))[1], -1, 1e-6);
        const auto view = aho::matrix::make_view(aho::d3::PointF{0, 0, 0}, aho::d3::PointF{0, 0, 1}, aho::d3::VectorF{0, 1, 0});
        EXPECT_EQ(aho::math::to_array(aho::matrix::transform_point(view, aho::d3::PointF{0, 0, 2})), (std::array{0.f, 0.f, 2.f}));
    }
} // namespace
