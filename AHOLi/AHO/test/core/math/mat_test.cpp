#include <AHO/core/vector.hpp>
#include <AHO/core/point.hpp>

#if defined(AHO_CONTRACT_MAT_INITIALIZER)
constexpr aho::Mat<int, 2, 2> initialized({{1, 2}, {3, 4}});
static_assert(initialized[0][0] == 1 && initialized[0][1] == 2 && initialized[1][0] == 3 && initialized[1][1] == 4);
#elif defined(AHO_CONTRACT_MAT_RECTANGULAR_FACTORY)
void check_factory() {
    constexpr std::array<int, 3> rows[2]{{1, 2, 3}, {4, 5, 6}};
    constexpr auto result = aho::matrix::make(rows);
    static_assert(std::same_as<decltype(result), const aho::Mat<int, 2, 3>>);
    static_assert(result[0][0] == 1 && result[0][1] == 2 && result[0][2] == 3 && result[1][0] == 4 && result[1][1] == 5 && result[1][2] == 6);
}
#elif defined(AHO_CONTRACT_MAT_AXIS_ROTATION)
void check_rotation() {
    const auto radians = aho::matrix::make_rotation<double>(aho::Radian{1}, aho::d3::VectorD(0, 0, 1));
    const auto degrees = aho::matrix::make_rotation<double>(aho::Degree{90}, aho::d3::VectorD(0, 0, 1));
}
#elif defined(AHO_CONTRACT_MAT_VIEW)
#include "../../support/matrix.hpp"
TEST(MatContractTest, ViewMatrixMovesEyeToOriginForFloatAndDouble) {
    auto verify = []<class T>() {
        using P = aho::_Point<T, vsl::D3>;
        using V = aho::_Vector<T, vsl::D3>;
        const auto view = aho::matrix::make_view(P(0, 0, 1), P(0, 0, 0), V(0, 1, 0));
        aho_test::expect_matrix(view, std::array<std::array<double, 4>, 4>{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, -1, 1}}});
    };
    verify.template operator()<float>();
    verify.template operator()<double>();
}
#elif defined(AHO_CONTRACT_MAT_AXIS_DEGREE_OVERLOAD)
using Rotation = aho::Mat4x4<double>(*)(aho::Degree, aho::d3::VectorD);
constexpr Rotation rotation = &aho::matrix::make_rotation<double>;
#elif defined(AHO_CONTRACT_MAT_PERSPECTIVE)
void check_perspective() {
    const auto radians = aho::matrix::make_perspective(aho::Radian{1}, 1.0, 0.1, 100.0);
    const auto degrees = aho::matrix::make_perspective(aho::Degree{90}, 1.0, 0.1, 100.0);
}
#elif defined(AHO_CONTRACT_MAT_SCALAR_RIGHT_MULTIPLY)
constexpr aho::Mat<int, 1, 2> matrix(std::array<std::array<int, 2>, 1>{{{2, 4}}});
constexpr auto result = matrix * 2;
static_assert(result[0][0] == 4 && result[0][1] == 8);
#elif defined(AHO_CONTRACT_MAT_SCALAR_LEFT_MULTIPLY)
constexpr aho::Mat<int, 1, 2> matrix(std::array<std::array<int, 2>, 1>{{{2, 4}}});
constexpr auto result = 2 * matrix;
static_assert(result[0][0] == 4 && result[0][1] == 8);
#elif defined(AHO_CONTRACT_MAT_SCALAR_RIGHT_DIVIDE)
constexpr aho::Mat<int, 1, 2> matrix(std::array<std::array<int, 2>, 1>{{{2, 4}}});
constexpr auto result = matrix / 2;
static_assert(result[0][0] == 1 && result[0][1] == 2);
#elif defined(AHO_CONTRACT_MAT_SCALAR_LEFT_DIVIDE)
constexpr aho::Mat<int, 1, 2> matrix(std::array<std::array<int, 2>, 1>{{{2, 4}}});
constexpr auto result = 8 / matrix;
static_assert(result[0][0] == 4 && result[0][1] == 2);
#else
#include "../../support/matrix.hpp"
#include <sstream>

namespace {
using aho_test::expect_identity;
using aho_test::expect_matrix;
using Matrix2 = aho::Mat<int, 2, 2>;
const std::array<std::array<int, 2>, 2> values{{{1, 2}, {3, 4}}};
const std::array<std::array<int, 2>, 2> others{{{5, 6}, {7, 8}}};
static_assert(Matrix2::type().row == 2 && Matrix2::type().column == 2);
static_assert(std::same_as<decltype(aho::matrix::inverse_gauss_jordan(aho::Mat<double, 2, 2>{})),
                          std::expected<aho::Mat<double, 2, 2>, aho::matrix::FailedCause>>);
TEST(MatTest, ArrayConstructorAndValueInitializationPreserveShapeAndValues) {
    const Matrix2 matrix(values);
    expect_matrix(matrix, values);
    expect_matrix(Matrix2{}, std::array<std::array<int, 2>, 2>{});
}
TEST(MatTest, UnaryAndBinaryAdditionAndSubtractionAreElementwise) {
    const Matrix2 a(values), b(others);
    expect_matrix(+a, values);
    expect_matrix(-a, std::array<std::array<int, 2>, 2>{{{-1, -2}, {-3, -4}}});
    expect_matrix(a + b, std::array<std::array<int, 2>, 2>{{{6, 8}, {10, 12}}});
    expect_matrix(a - b, std::array<std::array<int, 2>, 2>{{{-4, -4}, {-4, -4}}});
    expect_matrix(a, values);
}
TEST(MatTest, AssignmentAdditionMutatesAndReturnsOriginalObject) {
    Matrix2 a(values);
    EXPECT_EQ(&(a += Matrix2(others)), &a);
    expect_matrix(a, std::array<std::array<int, 2>, 2>{{{6, 8}, {10, 12}}});
}
TEST(MatTest, AssignmentSubtractionMutatesAndReturnsOriginalObject) {
    Matrix2 a(values);
    EXPECT_EQ(&(a -= Matrix2(others)), &a);
    expect_matrix(a, std::array<std::array<int, 2>, 2>{{{-4, -4}, {-4, -4}}});
}
TEST(MatTest, MatrixProductUsesRowColumnDotProducts) {
    const Matrix2 a(values), b(others);
    expect_matrix(a * b, std::array<std::array<int, 2>, 2>{{{19, 22}, {43, 50}}});
}
TEST(MatTest, AssignmentMultiplicationMatchesProductAndHandlesSelfAliasing) {
    Matrix2 a(values);
    EXPECT_EQ(&(a *= Matrix2(others)), &a);
    expect_matrix(a, std::array<std::array<int, 2>, 2>{{{19, 22}, {43, 50}}});
    Matrix2 b(values);
    b *= b;
    expect_matrix(b, std::array<std::array<int, 2>, 2>{{{7, 10}, {15, 22}}});
}
TEST(MatTest, RectangularProductHasExpectedShapeAndValues) {
    const aho::Mat<int, 2, 3> a(std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});
    const aho::Mat<int, 3, 1> b(std::array<std::array<int, 1>, 3>{{{2}, {3}, {4}}});
    expect_matrix(a * b, std::array<std::array<int, 1>, 2>{{{20}, {47}}});
}
TEST(MatTest, ScalarAssignmentsAreElementwise) {
    const Matrix2 a(values);
    const std::array<std::array<int, 2>, 2> doubled{{{2, 4}, {6, 8}}};
    auto mutable_matrix = a;
    expect_matrix(mutable_matrix *= 2, doubled);
    expect_matrix(mutable_matrix, doubled);
    expect_matrix(mutable_matrix /= 2, values);
    expect_matrix(mutable_matrix, values);
}
TEST(MatTest, CastConvertsEachElement) {
    Matrix2 matrix(values);
    expect_matrix(matrix.cast<double>(), std::array<std::array<double, 2>, 2>{{{1, 2}, {3, 4}}});
    aho::Mat<double, 1, 2> fractional(std::array<std::array<double, 2>, 1>{{{2.75, -3.25}}});
    expect_matrix(fractional.cast<int>(), std::array<std::array<int, 2>, 1>{{{2, -3}}});
}
TEST(MatFactoryTest, RowCopiesOriginalArray) {
    int input[]{1, 4, 7};
    const auto result = aho::matrix::row(input);
    input[0] = 99;
    EXPECT_EQ(result, (std::array<int, 3>{1, 4, 7}));
}
TEST(MatFactoryTest, IdentityFactoriesProduceUnitMatrices) {
    expect_identity(aho::matrix::make_identity<double, 3>());
    expect_identity(aho::matrix::make_identity<Matrix2>());
}
TEST(MatFactoryTest, InitializerFactoryStoresInputAsColumns) {
    const auto matrix = aho::matrix::make<int, 2, 2>({{1, 2}, {3, 4}});
    expect_matrix(matrix, std::array<std::array<int, 2>, 2>{{{1, 3}, {2, 4}}});
}
TEST(MatFactoryTest, ArrayFactoryCopiesSquareRowsWithoutTransposition) {
    const std::array<int, 2> rows[2]{{1, 2}, {3, 4}};
    expect_matrix(aho::matrix::make(rows), values);
}
TEST(MatFactoryTest, TranslationFactoryUsesHomogeneousRowVectorCoordinates) {
    const auto matrix = aho::matrix::make_move(aho::d3::VectorD(2, 3, 4));
    expect_matrix(matrix, std::array<std::array<double, 4>, 4>{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {2, 3, 4, 1}}});
}
TEST(MatFactoryTest, RadianRotationProducesKnownQuarterTurn) {
    const auto matrix = aho::matrix::make_rotation<double>(aho::Radian{std::numbers::pi / 2});
    expect_matrix(matrix, std::array<std::array<double, 2>, 2>{{{0, -1}, {1, 0}}});
}
TEST(MatFactoryTest, DegreeRotationProducesKnownQuarterTurn) {
    const auto matrix = aho::matrix::make_rotation<double>(aho::Degree{90});
    expect_matrix(matrix, std::array<std::array<double, 2>, 2>{{{0, -1}, {1, 0}}});
}
TEST(MatFactoryTest, AxisRotationNormalizesAxisAndPreservesHomogeneousCoordinate) {
    const auto matrix = aho::matrix::make_rotation<double>(aho::Radian{std::numbers::pi / 2}, aho::d3::VectorD(0, 0, 2));
    expect_matrix(matrix, std::array<std::array<double, 4>, 4>{{{0, -1, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}});
}
template<class T> class MatAxisRotationTest : public testing::Test {};
using RotationScalars = testing::Types<float, double>;
TYPED_TEST_SUITE(MatAxisRotationTest, RotationScalars);
TYPED_TEST(MatAxisRotationTest, DegreeOverloadHasTheDeclaredSignatureAndMatchesRadians) {
    using T = TypeParam;
    using V = aho::_Vector<T, vsl::D3>;
    using DegreeRotation = aho::Mat4x4<T>(*)(aho::Degree, V);
    constexpr DegreeRotation rotation = &aho::matrix::make_rotation<T>;
    const V axis(0, 0, 2);
    const auto degrees = rotation(aho::Degree{90}, axis);
    const auto radians = aho::matrix::make_rotation<T>(aho::Radian{std::numbers::pi / 2}, axis);
    const std::array<std::array<double, 4>, 4> expected{{{0, -1, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    expect_matrix(degrees, expected); expect_matrix(radians, expected);
    EXPECT_EQ(axis.value.z.value, 2);
}
TYPED_TEST(MatAxisRotationTest, AxisRotationsHaveKnownOrientationAndAreOrthogonal) {
    using T = TypeParam;
    using V = aho::_Vector<T, vsl::D3>;
    const auto x = aho::matrix::make_rotation<T>(aho::Degree{90}, V(2, 0, 0));
    const auto y = aho::matrix::make_rotation<T>(aho::Degree{90}, V(0, 3, 0));
    expect_matrix(x, std::array<std::array<double, 4>, 4>{{{1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}}});
    expect_matrix(y, std::array<std::array<double, 4>, 4>{{{0, 0, 1, 0}, {0, 1, 0, 0}, {-1, 0, 0, 0}, {0, 0, 0, 1}}});
    expect_identity(x * aho::matrix::make_rotation<T>(aho::Degree{-90}, V(2, 0, 0)));
    expect_identity(y * aho::matrix::make_rotation<T>(aho::Degree{-90}, V(0, 3, 0)));
}
TEST(MatFactoryTest, PerspectiveOverloadsProduceKnownProjectionAndInvertibleMatrix) {
    const auto radians = aho::matrix::make_perspective(aho::Radian{std::numbers::pi / 2}, 2.0, 1.0, 11.0);
    const auto degrees = aho::matrix::make_perspective(aho::Degree{90}, 2.0, 1.0, 11.0);
    const std::array<std::array<double, 4>, 4> expected{{{0.5, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, -1.2, -1}, {0, 0, -2.2, 0}}};
    expect_matrix(radians, expected); expect_matrix(degrees, expected);
    const auto inverse = aho::matrix::inverse_gauss_jordan(radians);
    ASSERT_TRUE(inverse);
    expect_identity(radians * *inverse);
}

template<class T> class MatInverseTest : public testing::Test {};
using NumericTypes = testing::Types<float, double>;
TYPED_TEST_SUITE(MatInverseTest, NumericTypes);
TYPED_TEST(MatInverseTest, GaussJordanReturnsKnownInverseAndPreservesInput) {
    const std::array<std::array<TypeParam, 2>, 2> original{{{4, 7}, {2, 6}}};
    const aho::Mat<TypeParam, 2, 2> matrix(original);
    const auto result = aho::matrix::inverse_gauss_jordan(matrix);
    ASSERT_TRUE(result.has_value());
    expect_matrix(*result, std::array<std::array<double, 2>, 2>{{{0.6, -0.7}, {-0.2, 0.4}}});
    expect_identity(matrix * *result);
    expect_identity(*result * matrix);
    expect_matrix(matrix, original);
}
TYPED_TEST(MatInverseTest, GaussJordanSwapsRowsForZeroPivot) {
    const aho::Mat<TypeParam, 2, 2> matrix(std::array<std::array<TypeParam, 2>, 2>{{{0, 2}, {4, 0}}});
    const auto result = aho::matrix::inverse_gauss_jordan(matrix);
    ASSERT_TRUE(result);
    expect_matrix(*result, std::array<std::array<double, 2>, 2>{{{0, 0.25}, {0.5, 0}}});
    expect_identity(matrix * *result);
}
TYPED_TEST(MatInverseTest, GaussJordanRejectsSingularAndNonSquareMatrices) {
    const aho::Mat<TypeParam, 2, 2> matrix(std::array<std::array<TypeParam, 2>, 2>{{{1, 2}, {2, 4}}});
    const auto singular = aho::matrix::inverse_gauss_jordan(matrix);
    ASSERT_FALSE(singular);
    EXPECT_EQ(singular.error(), aho::matrix::FailedCause::Singular);
    const auto non_square = aho::matrix::inverse_gauss_jordan(aho::Mat<TypeParam, 2, 3>{});
    ASSERT_FALSE(non_square);
    EXPECT_EQ(non_square.error(), aho::matrix::FailedCause::NotSquare);
}
TYPED_TEST(MatInverseTest, AffineInverseSupportsScaleShearTranslationAndReflection) {
    const aho::Mat<TypeParam, 4, 4> matrix(std::array<std::array<TypeParam, 4>, 4>{{
        {2, 1, 0, 4}, {0, 3, 0, 6}, {0, 0, -4, 8}, {0, 0, 0, 1}}});
    const auto result = aho::matrix::inverse_affine(matrix);
    ASSERT_TRUE(result);
    expect_matrix(*result, std::array<std::array<double, 4>, 4>{{
        {0.5, -1.0 / 6, 0, -1}, {0, 1.0 / 3, 0, -2}, {0, 0, -0.25, 2}, {0, 0, 0, 1}}});
    expect_identity(matrix * *result);
    expect_identity(*result * matrix);
    const auto general = aho::matrix::inverse_gauss_jordan(matrix);
    ASSERT_TRUE(general);
    expect_matrix(*result, static_cast<const typename decltype(matrix)::OriginArray&>(*general));
}
TYPED_TEST(MatInverseTest, AffineRejectsNonAffineAndSingularMatrices) {
    auto matrix = aho::matrix::make_identity<TypeParam, 4>();
    matrix[3][0] = 1;
    const auto non_affine = aho::matrix::inverse_affine(matrix);
    ASSERT_FALSE(non_affine);
    EXPECT_EQ(non_affine.error(), aho::matrix::FailedCause::NonAffine);
    matrix[3][0] = 0;
    matrix[0][0] = 0;
    const auto singular = aho::matrix::inverse_affine(matrix);
    ASSERT_FALSE(singular);
    EXPECT_EQ(singular.error(), aho::matrix::FailedCause::Singular);
}
TYPED_TEST(MatInverseTest, AffineSmallInvertibleScaleMustNotBeClassifiedAsSingular) {
    auto matrix = aho::matrix::make_identity<TypeParam, 4>();
    const TypeParam small = TypeParam{0.001};
    matrix[0][0] = small; matrix[1][1] = small; matrix[2][2] = small;
    const auto result = aho::matrix::inverse_affine(matrix);
    ASSERT_TRUE(result);
    expect_identity(matrix * *result);
}
TYPED_TEST(MatInverseTest, GaussJordanTinyWellConditionedMatrixMustRemainInvertible) {
    const aho::Mat<TypeParam, 1, 1> tiny(std::array<std::array<TypeParam, 1>, 1>{{{TypeParam{1e-8}}}});
    const auto general = aho::matrix::inverse_gauss_jordan(tiny);
    ASSERT_TRUE(general);
    expect_identity(tiny * *general);
}
TYPED_TEST(MatInverseTest, SmallEliminationFactorMustNotSkipSignificantContribution) {
    const aho::Mat<TypeParam, 2, 2> matrix(std::array<std::array<TypeParam, 2>, 2>{{{1, TypeParam{1e8}}, {TypeParam{1e-6}, 1}}});
    const auto result = aho::matrix::inverse_gauss_jordan(matrix);
    ASSERT_TRUE(result);
    expect_matrix(*result, std::array<std::array<double, 2>, 2>{{{-1.0 / 99, 1e8 / 99}, {1e-6 / 99, -1.0 / 99}}});
}
TEST(MatOutputTest, RectangularOutputContainsEveryElement) {
    const aho::Mat<int, 1, 2> matrix(std::array<std::array<int, 2>, 1>{{{11, 22}}});
    std::ostringstream stream;
    stream << matrix;
    EXPECT_NE(stream.str().find("11"), std::string::npos);
    EXPECT_NE(stream.str().find("22"), std::string::npos);
}
}
#endif
