#include <gtest/gtest.h>
#include <AHO/core/vector.hpp>
#if defined(AHO_CONTRACT_VECTOR_MIXED_ASSIGNMENT)
void verify() { aho::d2::VectorD a(1, 2); a += aho::d2::VectorF(2, 3); a *= 2.0f; }
#elif defined(AHO_CONTRACT_VECTOR_FLOAT_NORMALIZE)
void verify() { aho::d2::VectorF a(3, 4); a.normalize(); aho::d3::VectorF b(1, 2, 2); b.normalize(); }
#elif defined(AHO_CONTRACT_VECTOR_D1_PRODUCTS)
void verify() {
    aho::d1::VectorD a(2), b(3);
    static_assert(std::same_as<decltype(a.dot(b)), double>);
    static_assert(std::same_as<decltype(a.hadamard(b)), aho::d1::VectorD>);
}
#elif defined(AHO_CONTRACT_VECTOR_COMPARISON)
TEST(VectorContractTest, ThreeWayComparisonOrdersAllDimensionsAndRecognizesEquality) {
    const auto d1 = aho::d1::VectorD(1) <=> aho::d1::VectorD(2);
    const auto d2 = aho::d2::VectorD(1, 2) <=> aho::d2::VectorD(2, 3);
    const auto d3 = aho::d3::VectorD(1, 2, 3) <=> aho::d3::VectorD(2, 3, 4);
    EXPECT_TRUE(std::is_lt(d1)); EXPECT_TRUE(std::is_lt(d2)); EXPECT_TRUE(std::is_lt(d3));
    EXPECT_TRUE(aho::d1::VectorD(1) == aho::d1::VectorD(1));
    EXPECT_TRUE(aho::d2::VectorD(1, 2) == aho::d2::VectorD(1, 2));
    EXPECT_TRUE(aho::d3::VectorD(1, 2, 3) == aho::d3::VectorD(1, 2, 3));
}
#elif defined(AHO_CONTRACT_VECTOR_CUSTOM_COORDINATES)
void verify() {
    using CI = aho::coordinate::_MakeCoordinateInfo<true, false, true>::value;
    using V = aho::_Vector<double, vsl::D2, CI>;
    const V value(2, 3);
    static_assert(std::same_as<decltype(value * 2.0), V>);
    static_assert(std::same_as<decltype(value / 2.0), V>);
    const auto dot = value.dot(value);
    const auto cross = value.cross(value);
}
#elif defined(AHO_CONTRACT_VECTOR_CONSTEXPR_ARITHMETIC)
constexpr auto sum = aho::d2::VectorI(1, 2) + aho::d2::VectorI(3, 4);
static_assert(sum.value.x.value == 4 && sum.value.y.value == 6);
#else
#include "../support/geometry.hpp"
#include "../support/matrix.hpp"
namespace {
template<class T> class VectorTest : public testing::Test {};
TYPED_TEST_SUITE(VectorTest, aho_test::GeometryCases);
TYPED_TEST(VectorTest, DefaultScalarAndCoordinateConstructorsPreserveValues) {
    using V = typename TypeParam::Vector;
    aho_test::expect_coordinates(V{}, 0, 0, 0);
    const auto value = aho_test::geometry<V>(2, 3, 4);
    aho_test::expect_coordinates(value, 2, 3, 4);
    aho_test::expect_coordinates(V(value.value), 2, 3, 4);
}
TYPED_TEST(VectorTest, UnaryAdditionSubtractionAndCastPreserveCoordinates) {
    using V = typename TypeParam::Vector;
    const auto value = aho_test::geometry<V>(2, 3, 4);
    aho_test::expect_coordinates(+value, 2, 3, 4);
    aho_test::expect_coordinates(-value, -2, -3, -4);
    aho_test::expect_coordinates(value.template cast<double>(), 2, 3, 4);
}
TYPED_TEST(VectorTest, BinaryAdditionAndSubtractionAreElementwise) {
    using V = typename TypeParam::Vector;
    const auto a = aho_test::geometry<V>(2, 3, 4), b = aho_test::geometry<V>(1, 2, 3);
    aho_test::expect_coordinates(a + b, 3, 5, 7);
    aho_test::expect_coordinates(a - b, 1, 1, 1);
    aho_test::expect_coordinates(a, 2, 3, 4);
}
TYPED_TEST(VectorTest, ScalarMultiplicationAndDivisionSupportBothMultiplicationOrders) {
    using V = typename TypeParam::Vector;
    using T = typename TypeParam::element_type;
    const auto value = aho_test::geometry<V>(2, 4, 6);
    aho_test::expect_coordinates(value * T{2}, 4, 8, 12);
    aho_test::expect_coordinates(T{2} * value, 4, 8, 12);
    aho_test::expect_coordinates(value / T{2}, 1, 2, 3);
}
TYPED_TEST(VectorTest, CompoundOperatorsReturnAndMutateOriginalObject) {
    using V = typename TypeParam::Vector;
    using T = typename TypeParam::element_type;
    auto value = aho_test::geometry<V>(2, 4, 6);
    const auto delta = aho_test::geometry<V>(1, 2, 3);
    EXPECT_EQ(&(value += delta), &value); aho_test::expect_coordinates(value, 3, 6, 9);
    EXPECT_EQ(&(value -= delta), &value); aho_test::expect_coordinates(value, 2, 4, 6);
    EXPECT_EQ(&(value *= T{2}), &value); aho_test::expect_coordinates(value, 4, 8, 12);
    EXPECT_EQ(&(value /= T{2}), &value); aho_test::expect_coordinates(value, 2, 4, 6);
}
TYPED_TEST(VectorTest, LengthAndLessThanUseMagnitude) {
    using V = typename TypeParam::Vector;
    const auto value = aho_test::geometry<V>(-3, 4, 12);
    const double expected = TypeParam::dimension::dimension == 1 ? 3 : TypeParam::dimension::dimension == 2 ? 5 : 13;
    EXPECT_DOUBLE_EQ(value.length(), expected);
    EXPECT_TRUE(aho_test::geometry<V>(1) < value);
}
TEST(VectorProductsTest, TwoAndThreeDimensionalProductsMatchKnownValues) {
    const aho::d2::VectorD a(2, 3), b(4, 5);
    EXPECT_DOUBLE_EQ(a.dot(b), 23);
    EXPECT_DOUBLE_EQ(a.cross(b).value.z.value, -2);
    aho_test::expect_coordinates(a.hadamard(b), 8, 15);
    const aho::d3::VectorD x(1, 0, 0), y(0, 1, 0);
    EXPECT_DOUBLE_EQ(x.dot(y), 0);
    aho_test::expect_coordinates(x.cross(y), 0, 0, 1);
    aho_test::expect_coordinates(y.cross(x), 0, 0, -1);
    aho_test::expect_coordinates(aho::d3::VectorD(2, 3, 4).hadamard(aho::d3::VectorD(4, 5, 6)), 8, 15, 24);
    using namespace aho::product::dot;
    EXPECT_DOUBLE_EQ(a * b, 23);
}
TEST(VectorProductsTest, CrossAndHadamardOperatorNamespacesMatchMethods) {
    const aho::d3::VectorD x(1, 0, 0), y(0, 1, 0);
    { using namespace aho::product::cross; aho_test::expect_coordinates(x * y, 0, 0, 1); }
    { using namespace aho::product::hadamard; aho_test::expect_coordinates(x * y, 0, 0, 0); }
}
TEST(VectorNormalizationTest, PositiveAndNegativeOneDimensionalNormalizationPreservesDirection) {
    aho::d1::VectorD positive(4), negative(-4);
    EXPECT_EQ(&positive.normalize(), &positive);
    aho_test::expect_coordinates(positive, 1);
    aho_test::expect_coordinates(negative.get_normalized(), -1);
    negative.normalize();
    aho_test::expect_coordinates(negative, -1);
}
TEST(VectorNormalizationTest, TwoAndThreeDimensionalNormalizationHasUnitLength) {
    aho::d2::VectorD a(3, 4);
    EXPECT_DOUBLE_EQ(a.get_normalized().length(), 1);
    EXPECT_EQ(&a.normalize(), &a);
    aho_test::expect_coordinates(a, 0.6, 0.8);
    aho::d3::VectorD b(1, 2, 2);
    EXPECT_NEAR(b.get_normalized().length(), 1, 1e-14);
    EXPECT_EQ(&b.normalize(), &b);
    aho_test::expect_coordinates(b, 1.0 / 3, 2.0 / 3, 2.0 / 3);
}
TEST(VectorNormalizationTest, FloatingPointVectorsNormalizeWithinFloatRoundingTolerance) {
    aho::d2::VectorF a(3, 4);
    const auto copy = a.get_normalized();
    EXPECT_NEAR(copy.length(), 1, 1e-6);
    EXPECT_EQ(&a.normalize(), &a);
    aho_test::expect_coordinates(a, 0.6, 0.8);
    aho::d3::VectorF b(1, 2, 2);
    EXPECT_NEAR(b.get_normalized().length(), 1, 1e-6);
    EXPECT_EQ(&b.normalize(), &b);
    aho_test::expect_coordinates(b, 1.0 / 3, 2.0 / 3, 2.0 / 3);
}
TEST(VectorMatrixTest, RowMatrixConversionsPreserveCoordinatesAndHomogeneousZero) {
    aho_test::expect_matrix(static_cast<aho::Mat<double, 1, 1>>(aho::d1::VectorD(4)), std::array<std::array<double, 1>, 1>{{{4}}});
    aho_test::expect_matrix(static_cast<aho::Mat<double, 1, 2>>(aho::d2::VectorD(2, 3)), std::array<std::array<double, 2>, 1>{{{2, 3}}});
    aho_test::expect_matrix(static_cast<aho::Mat<double, 1, 4>>(aho::d3::VectorD(2, 3, 4)), std::array<std::array<double, 4>, 1>{{{2, 3, 4, 0}}});
}
TEST(VectorMatrixTest, TwoDimensionalSquareMatrixConversionPlacesCoordinatesOnDiagonal) {
    const auto matrix = static_cast<aho::Mat<double, 2, 2>>(aho::d2::VectorD(2, 3));
    aho_test::expect_matrix(matrix, std::array<std::array<double, 2>, 2>{{{2, 0}, {0, 3}}});
}
TEST(VectorMatrixTest, ThreeDimensionalSquareMatrixConversionPlacesCoordinatesOnDiagonal) {
    const auto matrix = static_cast<aho::Mat<double, 4, 4>>(aho::d3::VectorD(2, 3, 4));
    EXPECT_DOUBLE_EQ(matrix[0][0], 2); EXPECT_DOUBLE_EQ(matrix[1][1], 3); EXPECT_DOUBLE_EQ(matrix[2][2], 4);
}
TEST(VectorDeductionTest, DeductionAndDimensionWrappersConstructExpectedVectors) {
    const aho::Vector default_value;
    aho_test::expect_coordinates(default_value, 0);
    const aho::Vector single(2);
    const aho::Vector pair(2, 3);
    const aho::Vector triple(2, 3, 4);
    aho_test::expect_coordinates(single, 2);
    aho_test::expect_coordinates(pair, 2, 3);
    aho_test::expect_coordinates(triple, 2, 3, 4);
    using CI1 = aho::coordinate::_DefaultCoordinateInfo<vsl::D1>::value;
    using CI2 = aho::coordinate::_DefaultCoordinateInfo<vsl::D2>::value;
    using CI3 = aho::coordinate::_DefaultCoordinateInfo<vsl::D3>::value;
    aho_test::expect_coordinates(aho::d1::Vector<int, CI1>(2), 2);
    aho_test::expect_coordinates(aho::d2::Vector<int, CI2>(2, 3), 2, 3);
    aho_test::expect_coordinates(aho::d3::Vector<int, CI3>(2, 3, 4), 2, 3, 4);
    aho_test::expect_coordinates(aho::d1::Vector<int, CI1>{}, 0);
    aho_test::expect_coordinates(aho::d2::Vector<int, CI2>{}, 0, 0);
    aho_test::expect_coordinates(aho::d3::Vector<int, CI3>{}, 0, 0, 0);
}
TEST(VectorCastTest, FloatingPointToIntegerCastTruncatesTowardZeroInEveryDimension) {
    aho_test::expect_coordinates(aho::d1::VectorD(-2.75).cast<int>(), -2);
    aho_test::expect_coordinates(aho::d2::VectorD(-2.75, 3.75).cast<int>(), -2, 3);
    aho_test::expect_coordinates(aho::d3::VectorD(-2.75, 3.75, 4.5).cast<int>(), -2, 3, 4);
}
TEST(VectorConceptTest, UnrelatedTypesWithAliasesAreNotVectors) {
    struct PretendVector { using element_type = double; using dimension = vsl::D2; };
    EXPECT_FALSE(aho::concepts::is_vector<PretendVector>);
    EXPECT_TRUE(aho::concepts::is_vector<aho::d2::VectorD>);
}
}
#endif
