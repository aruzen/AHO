#include <AHO/core/vector.hpp>
#include <AHO/core/point.hpp>

#if defined(AHO_CONTRACT_MAT_INITIALIZER)
constexpr aho::Mat<int, 2, 2> initialized({{1, 2}, {3, 4}});
static_assert(initialized[0][0] == 1 && initialized[0][1] == 2 && initialized[1][0] == 3 && initialized[1][1] == 4);
constexpr aho::Mat<int, 2, 3> partial({{1}, {2, 3}});
static_assert(partial[0][0] == 1 && partial[0][1] == 0 && partial[0][2] == 0);
static_assert(partial[1][0] == 2 && partial[1][1] == 3 && partial[1][2] == 0);
constexpr aho::Mat<int, 2, 2> empty_rows({{}, {}});
static_assert(empty_rows[0][0] == 0 && empty_rows[1][1] == 0);
constexpr aho::Mat<int, 1, 2> truncated({{1, 2, 3}, {4, 5, 6}});
static_assert(truncated[0][0] == 1 && truncated[0][1] == 2);
constexpr aho::Mat<int, 0, 2> no_rows({{1, 2}});
constexpr aho::Mat<int, 2, 0> no_columns({{1}, {2}});
static_assert(no_rows.size() == 0 && no_columns.size() == 2);

#elif defined(AHO_CONTRACT_MAT_RECTANGULAR_FACTORY)
void check_factory() {
    constexpr std::array<int, 3> rows[2]{{1, 2, 3}, {4, 5, 6}};
    constexpr auto result = aho::matrix::make(rows);
    static_assert(std::same_as<decltype(result), const aho::Mat<int, 2, 3>>);
    static_assert(result[0][0] == 1 && result[0][1] == 2 && result[0][2] == 3 && result[1][0] == 4 && result[1][1] == 5 && result[1][2] == 6);
    constexpr std::array<int, 2> tall_rows[3]{{7, 8}, {9, 10}, {11, 12}};
    constexpr auto tall = aho::matrix::make(tall_rows);
    static_assert(std::same_as<decltype(tall), const aho::Mat<int, 3, 2>>);
    static_assert(tall[0][0] == 7 && tall[0][1] == 8 && tall[1][0] == 9 && tall[1][1] == 10 && tall[2][0] == 11 && tall[2][1] == 12);
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
        // AHO retains a 180-degree camera roll relative to the conventional right-handed lookAt.
        aho_test::expect_matrix(view, std::array<std::array<double, 4>, 4>{{{-1, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, 1, 0}, {0, 0, -1, 1}}});
    };
    verify.template operator()<float>();
    verify.template operator()<double>();
}
#elif defined(AHO_CONTRACT_MAT_AXIS_DEGREE_OVERLOAD)
using Rotation = aho::Mat4x4<double> (*)(aho::Degree, aho::d3::VectorD);
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
#include <gtest/gtest.h>
#include "../../support/one_way_cast.hpp"
namespace {
    TEST(MatOneWayCastTest, ConvertsEveryRectangularElementAndRejectsTheReverseConversion) {
        using M = aho::matrix::Mat<aho_test::CastSource, 2, 3>;
        static_assert(aho_test::HasCast<M, aho_test::CastTarget>);
        M source;
        source[0] = {aho_test::CastSource{1}, aho_test::CastSource{2}, aho_test::CastSource{3}};
        source[1] = {aho_test::CastSource{-4}, aho_test::CastSource{-5}, aho_test::CastSource{-6}};
        const auto converted = source.cast<aho_test::CastTarget>();
        using Converted = std::remove_cv_t<decltype(converted)>;
        static_assert(std::same_as<Converted, aho::matrix::Mat<aho_test::CastTarget, 2, 3>>);
        static_assert(!aho_test::HasCast<Converted, aho_test::CastSource>);
        const int expected[2][3]{{1, 2, 3}, {-4, -5, -6}};
        for (std::size_t row = 0; row < 2; ++row)
            for (std::size_t column = 0; column < 3; ++column) {
                EXPECT_EQ(converted[row][column].value, expected[row][column]);
                EXPECT_EQ(source[row][column].value, expected[row][column]);
            }
    }
} // namespace
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
    TEST(MatInitializationTest, InitializerListsPreserveRowsAndZeroFillMissingElements) {
        const aho::Mat<int, 2, 3> partial({{1}, {2, 3}});
        expect_matrix(partial, std::array<std::array<int, 3>, 2>{{{1, 0, 0}, {2, 3, 0}}});
        const aho::Mat<int, 3, 2> tall({{4, 5}, {}, {6}});
        expect_matrix(tall, std::array<std::array<int, 2>, 3>{{{4, 5}, {0, 0}, {6, 0}}});
        const aho::Mat<int, 2, 2> square({{1, 2}, {3, 4}});
        expect_matrix(square, values);
    }
    TEST(MatInitializationTest, ExcessElementsAreTruncatedAndZeroExtentsAreSafe) {
        const aho::Mat<int, 1, 2> truncated({{1, 2, 3}, {4, 5, 6}});
        expect_matrix(truncated, std::array<std::array<int, 2>, 1>{{{1, 2}}});
        const aho::Mat<int, 0, 2> no_rows({{1, 2}});
        const aho::Mat<int, 2, 0> no_columns({{1}, {2}});
        EXPECT_TRUE(no_rows.empty());
        ASSERT_EQ(no_columns.size(), 2u);
        EXPECT_TRUE(no_columns[0].empty());
        EXPECT_TRUE(no_columns[1].empty());
    }
    TEST(MatInitializationTest, ArrayFactoryPreservesWideAndTallMatrixShapes) {
        const std::array<int, 3> wide[2]{{1, 2, 3}, {4, 5, 6}};
        const std::array<int, 2> tall[3]{{7, 8}, {9, 10}, {11, 12}};
        expect_matrix(aho::matrix::make(wide), std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});
        expect_matrix(aho::matrix::make(tall), std::array<std::array<int, 2>, 3>{{{7, 8}, {9, 10}, {11, 12}}});
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
    TEST(MatCompoundAssignmentTest, SubtractionSupportsRectangularMatricesAndSelfAliasing) {
        using Matrix = aho::Mat<int, 2, 3>;
        const std::array<std::array<int, 3>, 2> left{{{1, 2, 3}, {4, 5, 6}}};
        const std::array<std::array<int, 3>, 2> right{{{6, 5, 4}, {3, 2, 1}}};
        Matrix matrix(left);
        EXPECT_EQ(&(matrix -= Matrix(right)), &matrix);
        expect_matrix(matrix, std::array<std::array<int, 3>, 2>{{{-5, -3, -1}, {1, 3, 5}}});
        EXPECT_EQ(&(matrix -= matrix), &matrix);
        expect_matrix(matrix, std::array<std::array<int, 3>, 2>{});
    }
    TEST(MatCompoundAssignmentTest, MultiplicationPreservesOperandsAndMatchesKnownProducts) {
        Matrix2 matrix(values);
        const Matrix2 right(others);
        EXPECT_EQ(&(matrix *= right), &matrix);
        expect_matrix(matrix, std::array<std::array<int, 2>, 2>{{{19, 22}, {43, 50}}});
        expect_matrix(right, others);
        const Matrix2 identity(std::array<std::array<int, 2>, 2>{{{1, 0}, {0, 1}}});
        matrix *= identity;
        expect_matrix(matrix, std::array<std::array<int, 2>, 2>{{{19, 22}, {43, 50}}});
        matrix *= Matrix2{};
        expect_matrix(matrix, std::array<std::array<int, 2>, 2>{});
    }
    TEST(MatCompoundAssignmentTest, MixedElementMultiplicationConvertsAfterAccumulatingEachDotProduct) {
        Matrix2 matrix(values);
        const aho::Mat<double, 2, 2> right(std::array<std::array<double, 2>, 2>{{{0.5, 0}, {0.5, 0}}});
        matrix *= right;
        expect_matrix(matrix, std::array<std::array<int, 2>, 2>{{{1, 0}, {3, 0}}});
    }
    TEST(MatCompoundAssignmentTest, IntegerOperationsRemainConstantEvaluable) {
        constexpr auto product = [] {
            Matrix2 matrix(std::array<std::array<int, 2>, 2>{{{1, 2}, {3, 4}}});
            matrix *= matrix;
            return matrix;
        }();
        static_assert(product[0][0] == 7 && product[0][1] == 10 && product[1][0] == 15 && product[1][1] == 22);
        constexpr auto difference = [] {
            Matrix2 matrix(std::array<std::array<int, 2>, 2>{{{1, 2}, {3, 4}}});
            matrix -= matrix;
            return matrix;
        }();
        static_assert(difference[0][0] == 0 && difference[0][1] == 0 && difference[1][0] == 0 && difference[1][1] == 0);
        expect_matrix(product, std::array<std::array<int, 2>, 2>{{{7, 10}, {15, 22}}});
        expect_matrix(difference, std::array<std::array<int, 2>, 2>{});
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
    template<class T>
    class MatConstCastTest : public testing::Test {};
    using ConstCastScalars = testing::Types<float, double, long double>;
    TYPED_TEST_SUITE(MatConstCastTest, ConstCastScalars);
    TYPED_TEST(MatConstCastTest, ConstRectangularMatricesConvertEveryElementWithoutChangingTheirSource) {
        using T = TypeParam;
        const std::array<std::array<T, 3>, 2> values{{{T{-2.75}, T{4.5}, 0}, {T{1.125}, T{-3.25}, 6}}};
        const aho::Mat<T, 2, 3> source(values);
        auto integers = source.template cast<int>();
        const auto doubles = source.template cast<double>();
        static_assert(std::same_as<decltype(integers), aho::Mat<int, 2, 3>>);
        static_assert(std::same_as<std::remove_cv_t<decltype(doubles)>, aho::Mat<double, 2, 3>>);
        expect_matrix(integers, std::array<std::array<int, 3>, 2>{{{-2, 4, 0}, {1, -3, 6}}});
        expect_matrix(doubles, std::array<std::array<double, 3>, 2>{{{-2.75, 4.5, 0}, {1.125, -3.25, 6}}});
        integers[0][0] = 99;
        expect_matrix(source, values);
    }
    TEST(MatConstCastTest, ConstMatricesWithZeroDimensionsKeepTheirShape) {
        const aho::Mat<int, 0, 3> no_rows{};
        const aho::Mat<int, 2, 0> no_columns{};
        const auto converted_rows = no_rows.cast<double>();
        const auto converted_columns = no_columns.cast<double>();
        static_assert(std::same_as<std::remove_cv_t<decltype(converted_rows)>, aho::Mat<double, 0, 3>>);
        static_assert(std::same_as<std::remove_cv_t<decltype(converted_columns)>, aho::Mat<double, 2, 0>>);
        EXPECT_TRUE(converted_rows.empty());
        EXPECT_EQ(converted_columns.size(), 2u);
        EXPECT_TRUE(converted_columns[0].empty());
        EXPECT_TRUE(converted_columns[1].empty());
    }

    template<class T>
    class MatScalarOperatorTest : public testing::Test {};
    using ScalarTypes = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(MatScalarOperatorTest, ScalarTypes);
    TYPED_TEST(MatScalarOperatorTest, LeftAndRightOperatorsUseOperandOrderAndKeepSourceUnchanged) {
        using T = TypeParam;
        constexpr std::array<std::array<T, 3>, 2> values{{{2, -4, 6}, {8, -12, 24}}};
        const aho::Mat<T, 2, 3> matrix(values);
        const std::array<std::array<T, 3>, 2> doubled{{{4, -8, 12}, {16, -24, 48}}};
        const std::array<std::array<T, 3>, 2> halved{{{1, -2, 3}, {4, -6, 12}}};
        const std::array<std::array<T, 3>, 2> reciprocal{{{12, -6, 4}, {3, -2, 1}}};
        expect_matrix(matrix * T{2}, doubled);
        expect_matrix(T{2} * matrix, doubled);
        expect_matrix(matrix / T{2}, halved);
        expect_matrix(T{24} / matrix, reciprocal);
        expect_matrix(matrix * T{0}, std::array<std::array<T, 3>, 2>{});
        expect_matrix(matrix, values);
    }
    template<class Element, class Scalar, class Result>
    struct MixedMatrixScalarCase {
        using element_type = Element;
        using scalar_type = Scalar;
        using result_type = Result;
    };
    using MixedMatrixScalars = testing::Types<
        MixedMatrixScalarCase<int, float, float>,
        MixedMatrixScalarCase<float, double, double>,
        MixedMatrixScalarCase<double, int, double>>;
    template<class T>
    class MatMixedScalarOperatorTest : public testing::Test {};
    TYPED_TEST_SUITE(MatMixedScalarOperatorTest, MixedMatrixScalars);
    TYPED_TEST(MatMixedScalarOperatorTest, BothOperandOrdersPromoteResultTypesAndPreserveFractions) {
        using E = typename TypeParam::element_type;
        using S = typename TypeParam::scalar_type;
        using Result = aho::Mat<typename TypeParam::result_type, 1, 2>;
        const aho::Mat<E, 1, 2> matrix(std::array<std::array<E, 2>, 1>{{{-3, 6}}});
        static_assert(std::same_as<decltype(matrix * S{2}), Result>);
        static_assert(std::same_as<decltype(S{2} * matrix), Result>);
        static_assert(std::same_as<decltype(matrix / S{2}), Result>);
        static_assert(std::same_as<decltype(S{3} / matrix), Result>);
        const std::array<std::array<double, 2>, 1> doubled{{{-6, 12}}};
        const std::array<std::array<double, 2>, 1> halved{{{-1.5, 3}}};
        const std::array<std::array<double, 2>, 1> reciprocal{{{-1, 0.5}}};
        expect_matrix(matrix * S{2}, doubled);
        expect_matrix(S{2} * matrix, doubled);
        expect_matrix(matrix / S{2}, halved);
        expect_matrix(S{3} / matrix, reciprocal);
        expect_matrix(matrix, std::array<std::array<double, 2>, 1>{{{-3, 6}}});
    }
    TEST(MatMixedScalarOperatorTest, NarrowIntegerMultiplicationUsesIntegerPromotion) {
        constexpr aho::Mat<short, 1, 1> matrix(std::array<std::array<short, 1>, 1>{{{30000}}});
        constexpr auto right = matrix * short{2};
        constexpr auto left = short{2} * matrix;
        static_assert(std::same_as<std::remove_cv_t<decltype(right)>, aho::Mat<int, 1, 1>>);
        static_assert(std::same_as<std::remove_cv_t<decltype(left)>, aho::Mat<int, 1, 1>>);
        static_assert(right[0][0] == 60000 && left[0][0] == 60000);
        EXPECT_EQ(right[0][0], 60000);
        EXPECT_EQ(left[0][0], 60000);
    }
    TEST(MatTest, CastConvertsEachElement) {
        Matrix2 matrix(values);
        expect_matrix(matrix.cast<double>(), std::array<std::array<double, 2>, 2>{{{1, 2}, {3, 4}}});
        aho::Mat<double, 1, 2> fractional(std::array<std::array<double, 2>, 1>{{{2.75, -3.25}}});
        expect_matrix(fractional.cast<int>(), std::array<std::array<int, 2>, 1>{{{2, -3}}});
    }
    template<class T>
    class MatScalarAssignmentTest : public testing::Test {};
    using AssignmentScalars = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(MatScalarAssignmentTest, AssignmentScalars);
    TYPED_TEST(MatScalarAssignmentTest, ReturnsTheOriginalMatrixAndSupportsChainedUpdates) {
        using T = TypeParam;
        using M = aho::Mat<T, 1, 3>;
        M matrix(std::array<std::array<T, 3>, 1>{{{6, -12, 18}}});
        static_assert(std::same_as<decltype(matrix *= T{2}), M&>);
        static_assert(std::same_as<decltype(matrix /= T{2}), M&>);
        EXPECT_EQ(&((matrix *= T{2}) *= T{3}), &matrix);
        expect_matrix(matrix, std::array<std::array<T, 3>, 1>{{{36, -72, 108}}});
        EXPECT_EQ(&((matrix /= T{2}) /= T{3}), &matrix);
        expect_matrix(matrix, std::array<std::array<T, 3>, 1>{{{6, -12, 18}}});
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
    template<class T>
    class MatListFactoryTest : public testing::Test {};
    using ListFactoryScalars = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(MatListFactoryTest, ListFactoryScalars);
    TYPED_TEST(MatListFactoryTest, RectangularInputsPreserveColumnOrdering) {
        using T = TypeParam;
        constexpr auto wide = aho::matrix::make<T, 2, 3>({{1, 4}, {2, 5}, {3, 6}});
        constexpr auto tall = aho::matrix::make<T, 3, 2>({{1, 3, 5}, {2, 4, 6}});
        static_assert(std::same_as<std::remove_cv_t<decltype(wide)>, aho::Mat<T, 2, 3>>);
        static_assert(std::same_as<std::remove_cv_t<decltype(tall)>, aho::Mat<T, 3, 2>>);
        static_assert(wide[0][2] == T{3} && wide[1][2] == T{6});
        static_assert(tall[2][0] == T{5} && tall[2][1] == T{6});
        expect_matrix(wide, std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});
        expect_matrix(tall, std::array<std::array<int, 2>, 3>{{{1, 2}, {3, 4}, {5, 6}}});
    }
    TYPED_TEST(MatListFactoryTest, MissingRowsAndColumnsAreZeroInitialized) {
        using T = TypeParam;
        const auto partial = aho::matrix::make<T, 2, 3>({{1}, {}, {3, 6}});
        expect_matrix(partial, std::array<std::array<int, 3>, 2>{{{1, 0, 3}, {0, 0, 6}}});
        expect_matrix(aho::matrix::make<T, 3, 2>({{1}}),
                      std::array<std::array<int, 2>, 3>{{{1, 0}, {0, 0}, {0, 0}}});
        expect_matrix(aho::matrix::make<T, 2, 2>({}), std::array<std::array<int, 2>, 2>{});
    }
    TYPED_TEST(MatListFactoryTest, ExcessRowsAndColumnsAreIgnored) {
        using T = TypeParam;
        const auto matrix = aho::matrix::make<T, 2, 3>({{1, 4, 7}, {2, 5, 8}, {3, 6, 9}, {10}});
        expect_matrix(matrix, std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});
    }
    TEST(MatListFactoryTest, ZeroDimensionsDoNotReadOrWriteElements) {
        const auto no_rows = aho::matrix::make<int, 0, 2>({{1}, {2}, {3}});
        const auto no_columns = aho::matrix::make<int, 2, 0>({{1}, {2}});
        EXPECT_TRUE(no_rows.empty());
        EXPECT_TRUE(no_columns[0].empty());
        EXPECT_TRUE(no_columns[1].empty());
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
    template<class T>
    class MatAxisRotationTest : public testing::Test {};
    using RotationScalars = testing::Types<float, double>;
    TYPED_TEST_SUITE(MatAxisRotationTest, RotationScalars);
    TYPED_TEST(MatAxisRotationTest, DegreeOverloadHasTheDeclaredSignatureAndMatchesRadians) {
        using T = TypeParam;
        using V = aho::_Vector<T, vsl::D3>;
        using DegreeRotation = aho::Mat4x4<T> (*)(aho::Degree, V);
        constexpr DegreeRotation rotation = &aho::matrix::make_rotation<T>;
        const V axis(0, 0, 2);
        const auto degrees = rotation(aho::Degree{90}, axis);
        const auto radians = aho::matrix::make_rotation<T>(aho::Radian{std::numbers::pi / 2}, axis);
        const std::array<std::array<double, 4>, 4> expected{{{0, -1, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
        expect_matrix(degrees, expected);
        expect_matrix(radians, expected);
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
        expect_matrix(radians, expected);
        expect_matrix(degrees, expected);
        const auto inverse = aho::matrix::inverse_gauss_jordan(radians);
        ASSERT_TRUE(inverse);
        expect_identity(radians * *inverse);
    }

    template<class T>
    class MatViewTest : public testing::Test {};
    using ViewScalars = testing::Types<float, double>;
    TYPED_TEST_SUITE(MatViewTest, ViewScalars);
    TYPED_TEST(MatViewTest, ExplicitCoordinateTypePreservesPrecisionAndExistingCameraBasis) {
        using T = TypeParam;
        using CI = aho::coordinate::_DefaultCoordinateInfo<vsl::D3>::value;
        using P = aho::_Point<T, vsl::D3, CI>;
        using V = aho::_Vector<T, vsl::D3, CI>;
        const P eye(1, 2, 3), target(1, 2, 2);
        const V up(0, 2, 0);
        using ViewFactory = aho::Mat4x4<T> (*)(P, P, V);
        constexpr ViewFactory factory = &aho::matrix::make_view<T, CI>;
        const auto view = factory(eye, target, up);
        static_assert(std::same_as<std::remove_cv_t<decltype(view)>, aho::Mat4x4<T>>);
        expect_matrix(view, std::array<std::array<double, 4>, 4>{{{-1, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, 1, 0}, {1, 2, -3, 1}}});
        EXPECT_EQ(eye.value.x.value, 1);
        EXPECT_EQ(eye.value.y.value, 2);
        EXPECT_EQ(eye.value.z.value, 3);
        EXPECT_EQ(up.value.y.value, 2);
    }
    TYPED_TEST(MatViewTest, RotatedCameraMovesEyeToOriginAndTargetToNegativeZ) {
        using T = TypeParam;
        using P = aho::_Point<T, vsl::D3>;
        using V = aho::_Vector<T, vsl::D3>;
        const auto view = aho::matrix::make_view(P(2, 0, 0), P(0, 0, 0), V(0, 1, 0));
        const aho::Mat<T, 1, 4> eye(std::array<std::array<T, 4>, 1>{{{2, 0, 0, 1}}});
        const aho::Mat<T, 1, 4> target(std::array<std::array<T, 4>, 1>{{{0, 0, 0, 1}}});
        expect_matrix(eye * view, std::array<std::array<double, 4>, 1>{{{0, 0, 0, 1}}});
        expect_matrix(target * view, std::array<std::array<double, 4>, 1>{{{0, 0, -2, 1}}});
        expect_matrix(view, std::array<std::array<double, 4>, 4>{{{0, 0, 1, 0}, {0, -1, 0, 0}, {1, 0, 0, 0}, {0, 0, -2, 1}}});
    }

    // Preserve the established sample orientation; positive viewport height maps NDC +Y downward.
    TYPED_TEST(MatViewTest, SampleCameraKeepsPositiveXRightAndPositiveZUp) {
        using T = TypeParam;
        using P = aho::_Point<T, vsl::D3>;
        using V = aho::_Vector<T, vsl::D3>;
        const auto view = aho::matrix::make_view(P(0, 2, 2), P(0, 0, 0), V(0, 0, 1));
        const auto projection = aho::matrix::make_perspective(aho::Degree{45}, T{1}, T{0.1}, T{10});
        const auto screen_position = [&](T x, T y, T z) {
            const aho::Mat<T, 1, 4> point(std::array<std::array<T, 4>, 1>{{{x, y, z, 1}}});
            const auto clip = point * view * projection;
            EXPECT_GT(clip[0][3], T{0});
            return std::array<T, 2>{(clip[0][0] / clip[0][3] + T{1}) / T{2},
                                    (clip[0][1] / clip[0][3] + T{1}) / T{2}};
        };
        const auto origin = screen_position(0, 0, 0);
        const auto positive_x = screen_position(1, 0, 0);
        const auto positive_z = screen_position(0, 0, 1);
        EXPECT_EQ(origin[0], T{0.5});
        EXPECT_EQ(origin[1], T{0.5});
        EXPECT_GT(positive_x[0], origin[0]);
        EXPECT_EQ(positive_x[1], origin[1]);
        EXPECT_EQ(positive_z[0], origin[0]);
        EXPECT_LT(positive_z[1], origin[1]);
    }

    template<class T>
    class MatInverseTest : public testing::Test {};
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
        const aho::Mat<TypeParam, 4, 4> matrix(std::array<std::array<TypeParam, 4>, 4>{{{2, 1, 0, 4}, {0, 3, 0, 6}, {0, 0, -4, 8}, {0, 0, 0, 1}}});
        const auto result = aho::matrix::inverse_affine(matrix);
        ASSERT_TRUE(result);
        expect_matrix(*result, std::array<std::array<double, 4>, 4>{{{0.5, -1.0 / 6, 0, -1}, {0, 1.0 / 3, 0, -2}, {0, 0, -0.25, 2}, {0, 0, 0, 1}}});
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
        matrix[0][0] = small;
        matrix[1][1] = small;
        matrix[2][2] = small;
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
    TYPED_TEST(MatInverseTest, NonzeroMachineEpsilonFactorsStillEliminateLargeContributions) {
        using T = TypeParam;
        for (const T sign : {T{1}, T{-1}}) {
            const T factor = sign * std::numeric_limits<T>::epsilon();
            const T coupling = T{4} / factor;
            SCOPED_TRACE(testing::Message() << "factor=" << factor);
            // The determinant is 1 - factor * coupling = -3, despite the small factor.
            const aho::Mat<T, 2, 2> matrix(std::array<std::array<T, 2>, 2>{{{1, coupling}, {factor, 1}}});
            const auto result = aho::matrix::inverse_gauss_jordan(matrix);
            ASSERT_TRUE(result);
            expect_matrix(*result, std::array<std::array<T, 2>, 2>{{{-T{1} / T{3}, coupling / T{3}}, {factor / T{3}, -T{1} / T{3}}}});
            EXPECT_EQ(matrix[0][1], coupling);
            EXPECT_EQ(matrix[1][0], factor);
        }
    }

    TEST(MatOutputTest, RectangularOutputContainsEveryElement) {
        const aho::Mat<int, 1, 2> matrix(std::array<std::array<int, 2>, 1>{{{11, 22}}});
        std::ostringstream stream;
        stream << matrix;
        EXPECT_NE(stream.str().find("11"), std::string::npos);
        EXPECT_NE(stream.str().find("22"), std::string::npos);
    }
    TEST(MatOutputTest, WideAndTallMatricesPrintRowsInOrderWithBalancedBraces) {
        const aho::Mat<int, 2, 3> wide(std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});
        const aho::Mat<int, 3, 2> tall(std::array<std::array<int, 2>, 3>{{{1, 2}, {3, 4}, {5, 6}}});
        std::ostringstream wide_output, tall_output;
        EXPECT_EQ(&(wide_output << wide), &wide_output);
        tall_output << tall;
        EXPECT_EQ(wide_output.str(), "{{1, 2, 3}\n{4, 5, 6}}\n");
        EXPECT_EQ(tall_output.str(), "{{1, 2}\n{3, 4}\n{5, 6}}\n");
    }
    TEST(MatOutputTest, SquareOutputDoesNotTransposeOffDiagonalElements) {
        const aho::Mat<int, 2, 2> matrix(std::array<std::array<int, 2>, 2>{{{1, 2}, {3, 4}}});
        std::ostringstream output;
        output << matrix;
        EXPECT_EQ(output.str(), "{{1, 2}\n{3, 4}}\n");
    }
    TEST(MatOutputTest, ZeroRowsAndZeroColumnsDoNotAccessElements) {
        const aho::Mat<int, 0, 3> no_rows{};
        const aho::Mat<int, 2, 0> no_columns{};
        const aho::Mat<int, 0, 0> empty{};
        std::ostringstream rows_output, columns_output, empty_output;
        rows_output << no_rows;
        columns_output << no_columns;
        empty_output << empty;
        EXPECT_EQ(rows_output.str(), "{}");
        EXPECT_EQ(columns_output.str(), "{{}\n{}}\n");
        EXPECT_EQ(empty_output.str(), "{}");
    }

} // namespace

namespace {
    template<class T>
    class MatScaledInverseTest : public testing::Test {};
    using ScaledInverseScalars = testing::Types<float, double, long double>;
    TYPED_TEST_SUITE(MatScaledInverseTest, ScaledInverseScalars);
    TYPED_TEST(MatScaledInverseTest, OneElementMatricesHaveRepresentableReciprocalsAtSmallAndLargeScales) {
        using T = TypeParam;
        const int limit = std::numeric_limits<T>::max_exponent;
        for (const int exponent : {-limit + 1, -limit / 2, 0, limit / 2, limit - 2})
            for (const T sign : {T{1}, T{-1}}) {
                SCOPED_TRACE(testing::Message() << "exponent=" << exponent << " sign=" << sign);
                const aho::Mat<T, 1, 1> matrix(std::array<std::array<T, 1>, 1>{{{std::ldexp(sign, exponent)}}});
                const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
                ASSERT_TRUE(inverse);
                EXPECT_EQ((*inverse)[0][0], std::ldexp(sign, -exponent));
                expect_identity(matrix * *inverse);
            }
    }
    TYPED_TEST(MatScaledInverseTest, IndependentRowAndColumnUnitsPreserveTheKnownInverseAndBothProducts) {
        using T = TypeParam;
        const int exponent = std::numeric_limits<T>::max_exponent / 4;
        const std::array<std::array<T, 2>, 2> base{{{2, 1}, {1, 1}}};
        const std::array<std::array<T, 2>, 2> knownInverse{{{1, -1}, {-1, 2}}};
        for (const int rowSign : {-1, 1})
            for (const int columnSign : {-1, 1}) {
                SCOPED_TRACE(testing::Message() << "rowSign=" << rowSign << " columnSign=" << columnSign);
                const std::array<int, 2> rows{rowSign * exponent, -rowSign * exponent};
                const std::array<int, 2> columns{columnSign * exponent, -columnSign * exponent};
                aho::Mat<T, 2, 2> matrix;
                for (std::size_t i = 0; i < 2; ++i)
                    for (std::size_t j = 0; j < 2; ++j)
                        matrix[i][j] = std::ldexp(base[i][j], rows[i] + columns[j]);
                const auto original = matrix;
                const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
                ASSERT_TRUE(inverse);
                for (std::size_t i = 0; i < 2; ++i)
                    for (std::size_t j = 0; j < 2; ++j)
                        EXPECT_EQ((*inverse)[i][j], std::ldexp(knownInverse[i][j], -columns[i] - rows[j]));
                expect_identity(matrix * *inverse);
                expect_identity(*inverse * matrix);
                EXPECT_EQ(matrix, original);
            }
    }
    TYPED_TEST(MatScaledInverseTest, ExtremeRowOrColumnUnitsKeepEveryInputAndFiniteInverseEntry) {
        using T = TypeParam;
        const int exponent = std::numeric_limits<T>::max_exponent - 2;
        const std::array<std::array<T, 2>, 2> base{{{1, 1}, {1, -1}}};
        const std::array<std::array<T, 2>, 2> knownInverse{{{T{0.5}, T{0.5}}, {T{0.5}, T{-0.5}}}};
        for (const bool scaleRows : {false, true}) {
            SCOPED_TRACE(testing::Message() << "scaleRows=" << scaleRows);
            const std::array<int, 2> rows = scaleRows ? std::array<int, 2>{exponent, -exponent} : std::array<int, 2>{};
            const std::array<int, 2> columns = scaleRows ? std::array<int, 2>{} : std::array<int, 2>{exponent, -exponent};
            aho::Mat<T, 2, 2> matrix;
            for (std::size_t i = 0; i < 2; ++i)
                for (std::size_t j = 0; j < 2; ++j)
                    matrix[i][j] = std::ldexp(base[i][j], rows[i] + columns[j]);
            const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
            ASSERT_TRUE(inverse);
            aho::Mat<T, 2, 2> normalizedInverse;
            for (std::size_t i = 0; i < 2; ++i)
                for (std::size_t j = 0; j < 2; ++j) {
                    EXPECT_TRUE(std::isfinite((*inverse)[i][j]));
                    EXPECT_EQ((*inverse)[i][j], std::ldexp(knownInverse[i][j], -columns[i] - rows[j]));
                    normalizedInverse[i][j] = std::ldexp((*inverse)[i][j], columns[i] + rows[j]);
                }
            // 元の単位の行列積自体がoverflowするため、既知の単位へ戻して両側の積を確認する。
            const aho::Mat<T, 2, 2> normalized(base);
            expect_identity(normalized * normalizedInverse);
            expect_identity(normalizedInverse * normalized);
        }
    }
    TYPED_TEST(MatScaledInverseTest, ExactlyDependentIntegerRowsRemainSingularAfterRescaling) {
        using T = TypeParam;
        for (const bool rescale : {false, true}) {
            std::uint32_t seed = 311;
            std::size_t accepted = 0;
            for (unsigned sample = 0; sample < 1000; ++sample) {
                aho::Mat<T, 3, 3> matrix;
                for (std::size_t i = 0; i < 2; ++i)
                    for (std::size_t j = 0; j < 3; ++j) {
                        seed = seed * 1664525u + 1013904223u;
                        matrix[i][j] = static_cast<T>(int((seed >> 16) % 19) - 9);
                    }
                for (std::size_t j = 0; j < 3; ++j)
                    matrix[2][j] = matrix[0][j] + matrix[1][j];
                if (rescale)
                    for (std::size_t i = 0; i < 3; ++i)
                        for (std::size_t j = 0; j < 3; ++j)
                            matrix[i][j] = std::ldexp(matrix[i][j], (int(i) - int(j)) * 20);
                const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
                if (inverse)
                    ++accepted;
                else
                    EXPECT_EQ(inverse.error(), aho::matrix::FailedCause::Singular);
            }
            EXPECT_EQ(accepted, 0u) << "rescale=" << rescale;
        }
    }
    TYPED_TEST(MatScaledInverseTest, LossyRowAndColumnScalingDoesNotEraseNonzeroInputs) {
        using T = TypeParam;
        const int exponent = std::numeric_limits<T>::max_exponent - 2;
        const T large = std::ldexp(T{1}, exponent), tiny = std::ldexp(T{1}, -exponent);
        const aho::Mat<T, 2, 2> matrix(std::array<std::array<T, 2>, 2>{{{large, tiny}, {tiny, large}}});
        const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
        ASSERT_TRUE(inverse);
        EXPECT_EQ((*inverse)[0][0], tiny);
        EXPECT_EQ((*inverse)[1][1], tiny);
        EXPECT_EQ((*inverse)[0][1], T{0});
        EXPECT_EQ((*inverse)[1][0], T{0});
        expect_identity(matrix * *inverse);
        expect_identity(*inverse * matrix);
        EXPECT_EQ(matrix[0][1], tiny);
        EXPECT_EQ(matrix[1][0], tiny);
    }
    TYPED_TEST(MatScaledInverseTest, UniformUnitsPreserveNearDependentRowRejection) {
        using T = TypeParam;
        // 可逆でもepsilonに近い相対pivotを持つ入力は、単位によらず数値的に特異と扱う。
        const T adjacent = std::nextafter(T{1}, T{2});
        for (const int exponent : {-30, 0, 30}) {
            SCOPED_TRACE(testing::Message() << "exponent=" << exponent);
            const T scale = std::ldexp(T{1}, exponent);
            const aho::Mat<T, 2, 2> matrix(std::array<std::array<T, 2>, 2>{{{scale, scale}, {scale, adjacent * scale}}});
            const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
            ASSERT_FALSE(inverse);
            EXPECT_EQ(inverse.error(), aho::matrix::FailedCause::Singular);
        }
    }
    TEST(MatScaledInverseTest, IntegerPermutationMatricesKeepTheirExactInverse) {
        const std::array<std::array<int, 2>, 2> values{{{0, 1}, {1, 0}}};
        const aho::Mat<int, 2, 2> matrix(values);
        const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
        ASSERT_TRUE(inverse);
        expect_matrix(*inverse, values);
        expect_identity(matrix * *inverse);
    }
    TEST(MatScaledInverseTest, EmptyMatricesRemainInvertibleAndZeroRowsRemainSingular) {
        const auto empty = aho::matrix::inverse_gauss_jordan(aho::Mat<double, 0, 0>{});
        ASSERT_TRUE(empty);
        EXPECT_TRUE(empty->empty());
        const auto zero = aho::matrix::inverse_gauss_jordan(aho::Mat<double, 2, 2>{});
        ASSERT_FALSE(zero);
        EXPECT_EQ(zero.error(), aho::matrix::FailedCause::Singular);
    }
    TYPED_TEST(MatScaledInverseTest, ScaledThreeByThreeMatricesPreserveTheirKnownInverse) {
        using T = TypeParam;
        const int exponent = std::numeric_limits<T>::max_exponent / 6;
        const std::array<int, 3> rows{-exponent, 0, exponent}, columns{exponent, -exponent, 0};
        const std::array<std::array<T, 3>, 3> base{{{1, 2, 3}, {0, 1, 4}, {0, 0, 1}}};
        const std::array<std::array<T, 3>, 3> knownInverse{{{1, -2, 5}, {0, 1, -4}, {0, 0, 1}}};
        aho::Mat<T, 3, 3> matrix;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                matrix[i][j] = std::ldexp(base[i][j], rows[i] + columns[j]);
        const auto inverse = aho::matrix::inverse_gauss_jordan(matrix);
        ASSERT_TRUE(inverse);
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                EXPECT_EQ((*inverse)[i][j], std::ldexp(knownInverse[i][j], -columns[i] - rows[j]));
        expect_identity(matrix * *inverse);
        expect_identity(*inverse * matrix);
    }
} // namespace

namespace {
    template<class T>
    class MatAffineScaleTest : public testing::Test {};
    using AffineScaleScalars = testing::Types<float, double, long double>;
    TYPED_TEST_SUITE(MatAffineScaleTest, AffineScaleScalars);
    TYPED_TEST(MatAffineScaleTest, UniformSmallAndLargeScalesPreserveReflectionAndTranslation) {
        using T = TypeParam;
        const int limit = std::numeric_limits<T>::max_exponent;
        for (const int exponent : {-limit / 2, -limit / 4, 0, limit / 4, limit / 2}) {
            SCOPED_TRACE(testing::Message() << "exponent=" << exponent);
            const T scale = std::ldexp(T{1}, exponent);
            auto matrix = aho::matrix::make_identity<T, 4>();
            auto knownInverse = aho::matrix::make_identity<T, 4>();
            const std::array<T, 3> diagonal{scale, 2 * scale, -scale};
            const std::array<T, 3> offset{1, -2, T{0.5}};
            for (std::size_t i = 0; i < 3; ++i) {
                matrix[i][i] = diagonal[i];
                matrix[i][3] = diagonal[i] * offset[i];
                knownInverse[i][i] = T{1} / diagonal[i];
                knownInverse[i][3] = -offset[i];
            }
            const auto original = matrix;
            const auto inverse = aho::matrix::inverse_affine(matrix);
            EXPECT_TRUE(inverse);
            if (!inverse)
                continue;
            EXPECT_EQ(*inverse, knownInverse);
            expect_identity(matrix * *inverse);
            expect_identity(*inverse * matrix);
            EXPECT_EQ(matrix, original);
        }
    }
    TYPED_TEST(MatAffineScaleTest, IndependentUnitsPreserveShearAndTranslatedKnownInverse) {
        using T = TypeParam;
        const int exponent = std::numeric_limits<T>::max_exponent / 6;
        const std::array<std::array<T, 3>, 3> base{{{1, 2, 3}, {0, 1, 4}, {0, 0, 1}}};
        const std::array<std::array<T, 3>, 3> knownInverse{{{1, -2, 5}, {0, 1, -4}, {0, 0, 1}}};
        for (const int rowSign : {-1, 1})
            for (const int columnSign : {-1, 1}) {
                SCOPED_TRACE(testing::Message() << "rowSign=" << rowSign << " columnSign=" << columnSign);
                const std::array<int, 3> rows{-rowSign * exponent, 0, rowSign * exponent};
                const std::array<int, 3> columns{columnSign * exponent, -columnSign * exponent, 0};
                auto matrix = aho::matrix::make_identity<T, 4>();
                auto expected = aho::matrix::make_identity<T, 4>();
                for (std::size_t i = 0; i < 3; ++i) {
                    for (std::size_t j = 0; j < 3; ++j) {
                        matrix[i][j] = std::ldexp(base[i][j], rows[i] + columns[j]);
                        expected[i][j] = std::ldexp(knownInverse[i][j], -columns[i] - rows[j]);
                    }
                    // t = A * e_zなので、逆変換の平行移動は-e_zと分かる。
                    matrix[i][3] = matrix[i][2];
                    expected[i][3] = i == 2 ? T{-1} : T{0};
                }
                const auto inverse = aho::matrix::inverse_affine(matrix);
                ASSERT_TRUE(inverse);
                EXPECT_EQ(*inverse, expected);
                expect_identity(matrix * *inverse);
                expect_identity(*inverse * matrix);
            }
    }
    TYPED_TEST(MatAffineScaleTest, DependentLinearRowsRemainSingularAcrossUnits) {
        using T = TypeParam;
        const int exponent = std::numeric_limits<T>::max_exponent / 4;
        const std::array<std::array<T, 3>, 3> base{{{1, 2, 3}, {2, 4, 6}, {0, 1, 1}}};
        for (const int power : {-exponent, 0, exponent}) {
            auto matrix = aho::matrix::make_identity<T, 4>();
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j)
                    matrix[i][j] = std::ldexp(base[i][j], power);
            const auto inverse = aho::matrix::inverse_affine(matrix);
            ASSERT_FALSE(inverse);
            EXPECT_EQ(inverse.error(), aho::matrix::FailedCause::Singular);
        }
    }
    TEST(MatAffineScaleTest, IntegerUnitDeterminantsKeepExactShearAndTranslation) {
        const aho::Mat<int, 4, 4> matrix(std::array<std::array<int, 4>, 4>{{{1, 2, 3, 2}, {0, 1, 4, -3}, {0, 0, 1, 5}, {0, 0, 0, 1}}});
        const aho::Mat<int, 4, 4> expected(std::array<std::array<int, 4>, 4>{{{1, -2, 5, -33}, {0, 1, -4, 23}, {0, 0, 1, -5}, {0, 0, 0, 1}}});
        const auto inverse = aho::matrix::inverse_affine(matrix);
        ASSERT_TRUE(inverse);
        EXPECT_EQ(*inverse, expected);
        expect_identity(matrix * *inverse);
        expect_identity(*inverse * matrix);
    }
} // namespace
#endif
