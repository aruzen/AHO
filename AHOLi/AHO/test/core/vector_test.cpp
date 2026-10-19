#include <gtest/gtest.h>
#include <AHO/core/vector.hpp>
#if defined(AHO_CONTRACT_VECTOR_MIXED_ASSIGNMENT)
void verify() {
    aho::d2::VectorD a(1, 2);
    a += aho::d2::VectorF(2, 3);
    a *= 2.0f;
}
#elif defined(AHO_CONTRACT_VECTOR_FLOAT_NORMALIZE)
void verify() {
    aho::d2::VectorF a(3, 4);
    a.normalize();
    aho::d3::VectorF b(1, 2, 2);
    b.normalize();
}
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
    EXPECT_TRUE(std::is_lt(d1));
    EXPECT_TRUE(std::is_lt(d2));
    EXPECT_TRUE(std::is_lt(d3));
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
#include "../support/one_way_cast.hpp"
namespace {
    template<class T>
    class VectorOneWayCastTest : public testing::Test {};
    using OneWayVectors = testing::Types<aho::_Vector<aho_test::CastSource, vsl::D1>,
                                         aho::_Vector<aho_test::CastSource, vsl::D2>, aho::_Vector<aho_test::CastSource, vsl::D3>,
                                         aho::_Vector<aho_test::CastSource, vsl::D2, aho::coordinate::_MakeCoordinateInfo<false, true, true>::value>>;
    TYPED_TEST_SUITE(VectorOneWayCastTest, OneWayVectors);
    TYPED_TEST(VectorOneWayCastTest, ChecksConversionFromTheStoredScalarToTheRequestedType) {
        static_assert(aho_test::HasCast<TypeParam, aho_test::CastTarget>);
        const TypeParam source = [] {
            if constexpr (TypeParam::dimension::dimension == 1)
                return TypeParam(aho_test::CastSource{2});
            else if constexpr (TypeParam::dimension::dimension == 2)
                return TypeParam(aho_test::CastSource{2}, aho_test::CastSource{-3});
            else
                return TypeParam(aho_test::CastSource{2}, aho_test::CastSource{-3}, aho_test::CastSource{5});
        }();
        const auto converted = source.template cast<aho_test::CastTarget>();
        using Converted = std::remove_cv_t<decltype(converted)>;
        static_assert(!aho_test::HasCast<Converted, aho_test::CastSource>);
        static_assert(std::same_as<typename Converted::coordinate_info, typename TypeParam::coordinate_info>);
        auto check = [](const auto& from, const auto& to, int expected) {
            EXPECT_EQ(from.value.value, expected);
            EXPECT_EQ(to.value.value, expected);
        };
        if constexpr (TypeParam::coordinate_info::EnabledX)
            check(source.value.x, converted.value.x, 2);
        if constexpr (TypeParam::coordinate_info::EnabledY)
            check(source.value.y, converted.value.y, TypeParam::coordinate_info::EnabledX ? -3 : 2);
        if constexpr (TypeParam::coordinate_info::EnabledZ)
            check(source.value.z, converted.value.z, TypeParam::dimension::dimension == 3 ? 5 : -3);
    }
} // namespace
#include "../support/geometry.hpp"
#include "../support/matrix.hpp"
namespace {
    template<class T>
    class VectorDefaultCoordinatesTest : public testing::Test {};
    using DefaultVectorScalars = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(VectorDefaultCoordinatesTest, DefaultVectorScalars);
    TYPED_TEST(VectorDefaultCoordinatesTest, DimensionWrappersUseMatchingDefaultCoordinatesAndDeduceScalarTypes) {
        using T = TypeParam;
        constexpr aho::d1::Vector<T> single(T{2});
        constexpr aho::d2::Vector<T> plane(T{3}, T{4});
        constexpr aho::d3::Vector<T> space(T{2}, T{3}, T{6});
        static_assert(std::same_as<typename decltype(single)::coordinate_info,
                                   aho::coordinate::_DefaultCoordinateInfo<vsl::D1>::value>);
        static_assert(std::same_as<typename decltype(plane)::coordinate_info,
                                   aho::coordinate::_DefaultCoordinateInfo<vsl::D2>::value>);
        static_assert(std::same_as<typename decltype(space)::coordinate_info,
                                   aho::coordinate::_DefaultCoordinateInfo<vsl::D3>::value>);
        static_assert(single.value.x.value == T{2});
        static_assert(plane.value.x.value == T{3} && plane.value.y.value == T{4});
        static_assert(space.value.x.value == T{2} && space.value.y.value == T{3} && space.value.z.value == T{6});
        const aho::d1::Vector inferred_single(T{2});
        const aho::d2::Vector inferred_plane(T{3}, T{4});
        const aho::d3::Vector inferred_space(T{2}, T{3}, T{6});
        static_assert(std::same_as<std::remove_cv_t<decltype(inferred_single)>, aho::d1::Vector<T>>);
        static_assert(std::same_as<std::remove_cv_t<decltype(inferred_plane)>, aho::d2::Vector<T>>);
        static_assert(std::same_as<std::remove_cv_t<decltype(inferred_space)>, aho::d3::Vector<T>>);
        aho_test::expect_coordinates(inferred_single, 2);
        aho_test::expect_coordinates(inferred_plane, 3, 4);
        aho_test::expect_coordinates(inferred_space, 2, 3, 6);
        aho_test::expect_coordinates(aho::d1::Vector<T>{}, 0);
        aho_test::expect_coordinates(aho::d2::Vector<T>{}, 0, 0);
        aho_test::expect_coordinates(aho::d3::Vector<T>{}, 0, 0, 0);
    }
    TEST(VectorDefaultCoordinatesTest, ExplicitCoordinateInformationStillSelectsCustomAxes) {
        using Y = aho::coordinate::_MakeCoordinateInfo<false, true, false>::value;
        using YZ = aho::coordinate::_MakeCoordinateInfo<false, true, true>::value;
        const aho::d1::Vector<int, Y> y(7);
        const aho::d2::Vector<double, YZ> yz(3, 4);
        static_assert(std::same_as<typename decltype(y)::coordinate_info, Y>);
        static_assert(std::same_as<typename decltype(yz)::coordinate_info, YZ>);
        EXPECT_EQ(y.value.y.value, 7);
        EXPECT_EQ(yz.value.y.value, 3);
        EXPECT_EQ(yz.value.z.value, 4);
    }

    template<class T>
    class VectorTest : public testing::Test {};
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
        EXPECT_EQ(&(value += delta), &value);
        aho_test::expect_coordinates(value, 3, 6, 9);
        EXPECT_EQ(&(value -= delta), &value);
        aho_test::expect_coordinates(value, 2, 4, 6);
        EXPECT_EQ(&(value *= T{2}), &value);
        aho_test::expect_coordinates(value, 4, 8, 12);
        EXPECT_EQ(&(value /= T{2}), &value);
        aho_test::expect_coordinates(value, 2, 4, 6);
    }
    TEST(VectorLengthRangeTest, LargeIntegerCoordinatesHaveFiniteKnownLengths) {
        const aho::d2::VectorI plane(60000, -80000);
        const aho::d3::VectorI space(20000, -30000, 60000);
        EXPECT_DOUBLE_EQ(plane.length(), 100000);
        EXPECT_DOUBLE_EQ(space.length(), 70000);
    }
    template<class T>
    class VectorFloatingLengthRangeTest : public testing::Test {};
    using VectorFloatingLengthScalars = testing::Types<float, double>;
    TYPED_TEST_SUITE(VectorFloatingLengthRangeTest, VectorFloatingLengthScalars);
    TYPED_TEST(VectorFloatingLengthRangeTest, LargeAndSmallCoordinatesAvoidIntermediateOverflowAndUnderflow) {
        using T = TypeParam;
        // Scaled 3-4-5 and 2-3-6-7 triples need only a few rounded operations.
        constexpr T tolerance = 32 * std::numeric_limits<T>::epsilon();
        constexpr T large = static_cast<T>(std::is_same_v<T, float> ? 1e30 : 1e200);
        constexpr T small = static_cast<T>(std::is_same_v<T, float> ? 1e-30 : 1e-200);
        for (const T scale : {large, small}) {
            const aho::_Vector<T, vsl::D2> plane(-T{3} * scale, T{4} * scale);
            const aho::_Vector<T, vsl::D3> space(T{2} * scale, -T{3} * scale, T{6} * scale);
            EXPECT_TRUE(std::isfinite(plane.length()));
            EXPECT_TRUE(std::isfinite(space.length()));
            EXPECT_GT(plane.length(), 0);
            EXPECT_GT(space.length(), 0);
            EXPECT_NEAR(plane.length() / scale, 5, tolerance);
            EXPECT_NEAR(space.length() / scale, 7, tolerance);
            EXPECT_NEAR(plane.get_normalized().length(), 1, tolerance);
            EXPECT_NEAR(space.get_normalized().length(), 1, tolerance);
        }
    }
    TYPED_TEST(VectorTest, LengthAndLessThanUseMagnitude) {
        using V = typename TypeParam::Vector;
        const auto value = aho_test::geometry<V>(-3, 4, 12);
        const double expected = TypeParam::dimension::dimension == 1 ? 3 : TypeParam::dimension::dimension == 2 ? 5
                                                                                                                : 13;
        EXPECT_DOUBLE_EQ(value.length(), expected);
        EXPECT_TRUE(aho_test::geometry<V>(1) < value);
    }
    template<class T>
    class VectorOneDimensionalProductTest : public testing::Test {};
    using ProductScalars = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(VectorOneDimensionalProductTest, ProductScalars);
    TYPED_TEST(VectorOneDimensionalProductTest, DotAndHadamardUseScalarProductsAndPreserveInputs) {
        using V = aho::_Vector<TypeParam, vsl::D1>;
        const V left(-2), right(3), zero(0);
        const auto dot = left.dot(right);
        const auto hadamard = left.hadamard(right);
        static_assert(std::same_as<std::remove_cv_t<decltype(dot)>, TypeParam>);
        static_assert(std::same_as<std::remove_cv_t<decltype(hadamard)>, V>);
        EXPECT_EQ(dot, -6);
        aho_test::expect_coordinates(hadamard, -6);
        EXPECT_EQ(left.dot(left), 4);
        EXPECT_EQ(left.dot(zero), 0);
        aho_test::expect_coordinates(left.hadamard(zero), 0);
        {
            using namespace aho::product::dot;
            EXPECT_EQ(left * right, -6);
        }
        {
            using namespace aho::product::hadamard;
            aho_test::expect_coordinates(left * right, -6);
        }
        aho_test::expect_coordinates(left, -2);
        aho_test::expect_coordinates(right, 3);
    }
    TEST(VectorOneDimensionalProductTest, MixedScalarsPromoteTheResultElementType) {
        const aho::d1::VectorI integer(-2);
        const aho::d1::VectorF fractional(1.25f);
        const auto product = integer.hadamard(fractional);
        static_assert(std::same_as<std::remove_cv_t<decltype(product)>, aho::d1::VectorF>);
        static_assert(std::same_as<decltype(integer.dot(fractional)), float>);
        static_assert(std::same_as<decltype(fractional.dot(integer)), float>);
        EXPECT_FLOAT_EQ(integer.dot(fractional), -2.5f);
        EXPECT_FLOAT_EQ(fractional.dot(integer), -2.5f);
        aho_test::expect_coordinates(product, -2.5);
        aho_test::expect_coordinates(fractional.hadamard(integer), -2.5);
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
        {
            using namespace aho::product::cross;
            aho_test::expect_coordinates(x * y, 0, 0, 1);
        }
        {
            using namespace aho::product::hadamard;
            aho_test::expect_coordinates(x * y, 0, 0, 0);
        }
    }
    template<class T>
    class VectorThreeDimensionalInputTest : public testing::Test {};
    TYPED_TEST_SUITE(VectorThreeDimensionalInputTest, ProductScalars);
    TYPED_TEST(VectorThreeDimensionalInputTest, ProductsAcceptDerivedVectorInputsAndPreserveValues) {
        using V = aho::_Vector<TypeParam, vsl::D3>;
        using Derived = aho::d3::Vector<TypeParam, typename V::coordinate_info>;
        const V left(2, 3, 4);
        const Derived right(4, 5, 6);
        const auto cross = left.cross(right);
        const auto hadamard = left.hadamard(right);
        static_assert(std::same_as<std::remove_cv_t<decltype(cross)>, V>);
        static_assert(std::same_as<std::remove_cv_t<decltype(hadamard)>, V>);
        static_assert(std::same_as<decltype(left.dot(right)), TypeParam>);
        EXPECT_EQ(left.dot(right), 47);
        aho_test::expect_coordinates(cross, -2, 4, -2);
        aho_test::expect_coordinates(hadamard, 8, 15, 24);
        aho_test::expect_coordinates(right.cross(left), 2, -4, 2);
        aho_test::expect_coordinates(left, 2, 3, 4);
        aho_test::expect_coordinates(right, 4, 5, 6);
    }
    TEST(VectorThreeDimensionalInputTest, MixedScalarProductsPromoteFractionalResultsInBothOrders) {
        const aho::d3::VectorI integer(1, 2, 3);
        using CI = aho::d3::VectorF::coordinate_info;
        const aho::d3::Vector<float, CI> fractional(0.5f, 1.5f, 2.5f);
        const auto cross = integer.cross(fractional);
        const auto reverse = fractional.cross(integer);
        const auto hadamard = integer.hadamard(fractional);
        static_assert(std::same_as<std::remove_cv_t<decltype(cross)>, aho::d3::VectorF>);
        static_assert(std::same_as<std::remove_cv_t<decltype(reverse)>, aho::d3::VectorF>);
        static_assert(std::same_as<std::remove_cv_t<decltype(hadamard)>, aho::d3::VectorF>);
        static_assert(std::same_as<decltype(integer.dot(fractional)), float>);
        static_assert(std::same_as<decltype(fractional.dot(integer)), float>);
        EXPECT_FLOAT_EQ(integer.dot(fractional), 11.0f);
        EXPECT_FLOAT_EQ(fractional.dot(integer), 11.0f);
        aho_test::expect_coordinates(cross, 0.5, -1, 0.5);
        aho_test::expect_coordinates(reverse, -0.5, 1, -0.5);
        aho_test::expect_coordinates(hadamard, 0.5, 3, 7.5);
        aho_test::expect_coordinates(fractional.hadamard(integer), 0.5, 3, 7.5);
        aho_test::expect_coordinates(integer, 1, 2, 3);
        aho_test::expect_coordinates(fractional, 0.5, 1.5, 2.5);
    }
    TEST(VectorNormalizationTest, PositiveAndNegativeOneDimensionalNormalizationPreservesDirection) {
        aho::d1::VectorD positive(4), negative(-4);
        EXPECT_EQ(&positive.normalize(), &positive);
        aho_test::expect_coordinates(positive, 1);
        aho_test::expect_coordinates(negative.get_normalized(), -1);
        negative.normalize();
        aho_test::expect_coordinates(negative, -1);
    }
    template<class T>
    class VectorNormalization1DTest : public testing::Test {};
    using Normalization1DTypes = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(VectorNormalization1DTest, Normalization1DTypes);
    TYPED_TEST(VectorNormalization1DTest, CopyAndInPlaceNormalizationPreserveDirectionIncludingExtremeValues) {
        using V = aho::_Vector<TypeParam, vsl::D1>;
        const std::array<TypeParam, 4> values{TypeParam(1), TypeParam(-1),
                                              std::numeric_limits<TypeParam>::max(), std::numeric_limits<TypeParam>::lowest()};
        for (const auto value : values) {
            const V source(value);
            const auto normalized = source.get_normalized();
            const auto expected = value < TypeParam{} ? -1 : 1;
            EXPECT_EQ(normalized.value.x.value, expected);
            EXPECT_EQ(source.value.x.value, value);
            auto mutable_value = source;
            EXPECT_EQ(&mutable_value.normalize(), &mutable_value);
            EXPECT_EQ(mutable_value.value.x.value, expected);
        }
    }
    TEST(VectorNormalizationTest, OneDimensionalFractionalValuesPreserveDirectionAndMagnitudeIsOne) {
        const aho::d1::VectorD negative(-0.125), positive(0.25);
        aho_test::expect_coordinates(negative.get_normalized(), -1);
        aho_test::expect_coordinates(positive.get_normalized(), 1);
    }
    TEST(VectorNormalizationTest, OneDimensionalNormalizationSupportsEachAxisInConstantEvaluation) {
        auto verify = []<bool X, bool Y, bool Z>() {
            using CI = aho::coordinate::_MakeCoordinateInfo<X, Y, Z>::value;
            using V = aho::_Vector<int, vsl::D1, CI>;
            constexpr V source(-4);
            constexpr auto result = source.get_normalized();
            if constexpr (X) {
                static_assert(result.value.x.value == -1 && source.value.x.value == -4);
                EXPECT_EQ(result.value.x.value, -1);
            } else if constexpr (Y) {
                static_assert(result.value.y.value == -1 && source.value.y.value == -4);
                EXPECT_EQ(result.value.y.value, -1);
            } else {
                static_assert(result.value.z.value == -1 && source.value.z.value == -4);
                EXPECT_EQ(result.value.z.value, -1);
            }
        };
        verify.template operator()<true, false, false>();
        verify.template operator()<false, true, false>();
        verify.template operator()<false, false, true>();
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
    template<class T>
    class VectorOneDimensionalLengthTest : public testing::Test {};
    using VectorLengthScalars = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(VectorOneDimensionalLengthTest, VectorLengthScalars);
    TYPED_TEST(VectorOneDimensionalLengthTest, LengthUsesTheEnabledAxisAndPreservesInputs) {
        using T = TypeParam;
        using X = aho::coordinate::_MakeCoordinateInfo<true, false, false>::value;
        using Y = aho::coordinate::_MakeCoordinateInfo<false, true, false>::value;
        using Z = aho::coordinate::_MakeCoordinateInfo<false, false, true>::value;
        []<class... Axes>(std::tuple<Axes...>) {
            ([]<class Axis>() {
                using V = aho::_Vector<T, vsl::D1, Axis>;
                const V negative(T{-3});
                EXPECT_EQ(negative.length(), 3);
                EXPECT_EQ(V(T{3}).length(), 3);
                EXPECT_EQ(V(T{}).length(), 0);
                if constexpr (Axis::EnabledX)
                    EXPECT_EQ(negative.value.x.value, -3);
                else if constexpr (Axis::EnabledY)
                    EXPECT_EQ(negative.value.y.value, -3);
                else
                    EXPECT_EQ(negative.value.z.value, -3);
            }.template operator()<Axes>(),
             ...);
        }(std::tuple<X, Y, Z>{});
    }
    TEST(VectorOneDimensionalLengthTest, MinimumIntegerCoordinateHasRepresentablePositiveLength) {
        constexpr aho::d1::VectorI negative(-3);
        static_assert(negative.length() == 3);
        static_assert(std::same_as<decltype(negative.length()), unsigned>);
        static_assert(std::same_as<decltype(aho::d1::VectorF(-3).length()), float>);
        const aho::d1::VectorI minimum(std::numeric_limits<int>::min());
        EXPECT_EQ(minimum.length(), -static_cast<double>(std::numeric_limits<int>::min()));
    }
    TEST(VectorOneDimensionalLengthTest, UnsignedCoordinatesAreAcceptedAndNegativeZeroHasPositiveLengthZero) {
        const aho::_Vector<unsigned, vsl::D1> maximum(std::numeric_limits<unsigned>::max());
        EXPECT_EQ(maximum.length(), static_cast<double>(std::numeric_limits<unsigned>::max()));
        const aho::d1::VectorD zero(-0.0);
        EXPECT_EQ(zero.length(), 0);
        EXPECT_FALSE(std::signbit(zero.length()));
    }
    TEST(VectorOneDimensionalLengthTest, LargeIntegralMagnitudesRetainAdjacentValuesAndOrdering) {
        using V = aho::_Vector<long long, vsl::D1>;
        const V smaller(9007199254740992LL), larger(9007199254740993LL);
        EXPECT_EQ(smaller.length(), 9007199254740992ULL);
        EXPECT_EQ(larger.length(), 9007199254740993ULL);
        EXPECT_TRUE(smaller < larger);
        EXPECT_EQ(V(std::numeric_limits<long long>::min()).length(), 9223372036854775808ULL);
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
        EXPECT_DOUBLE_EQ(matrix[0][0], 2);
        EXPECT_DOUBLE_EQ(matrix[1][1], 3);
        EXPECT_DOUBLE_EQ(matrix[2][2], 4);
    }
    template<class T, bool X, bool Y, bool Z>
    struct VectorPlaneCase {
        using Info = aho::coordinate::_MakeCoordinateInfo<X, Y, Z>::value;
        using Vector = aho::_Vector<T, vsl::D2, Info>;
    };
    using VectorPlanes = testing::Types<
        VectorPlaneCase<int, true, true, false>, VectorPlaneCase<int, true, false, true>, VectorPlaneCase<int, false, true, true>,
        VectorPlaneCase<float, true, true, false>, VectorPlaneCase<float, true, false, true>, VectorPlaneCase<float, false, true, true>,
        VectorPlaneCase<double, true, true, false>, VectorPlaneCase<double, true, false, true>, VectorPlaneCase<double, false, true, true>>;
    template<class T>
    class VectorPlaneTest : public testing::Test {};
    TYPED_TEST_SUITE(VectorPlaneTest, VectorPlanes);
    template<class V>
    void expect_plane(const V& value, double first, double second) {
        using CI = typename V::coordinate_info;
        if constexpr (CI::EnabledX)
            EXPECT_EQ(value.value.x.value, first);
        if constexpr (CI::EnabledY)
            EXPECT_EQ(value.value.y.value, CI::EnabledX ? second : first);
        if constexpr (CI::EnabledZ)
            EXPECT_EQ(value.value.z.value, second);
    }
    TYPED_TEST(VectorPlaneTest, ScalarOperatorsKeepThePlaneAndSourceCoordinates) {
        using V = typename TypeParam::Vector;
        const V value(2, -4);
        static_assert(std::same_as<decltype(value * 2), V>);
        static_assert(std::same_as<decltype(2 * value), V>);
        static_assert(std::same_as<decltype(value / 2), V>);
        expect_plane(value * 2, 4, -8);
        expect_plane(2 * value, 4, -8);
        expect_plane(value / 2, 1, -2);
        expect_plane(value, 2, -4);
    }
    TYPED_TEST(VectorPlaneTest, DotAndHadamardAcceptVectorsOnTheSamePlane) {
        using V = typename TypeParam::Vector;
        const V left(2, 3), right(4, 5);
        static_assert(std::same_as<decltype(left.hadamard(right)), V>);
        EXPECT_EQ(left.dot(right), 23);
        EXPECT_EQ(right.dot(left), 23);
        expect_plane(left.hadamard(right), 8, 15);
    }
    TYPED_TEST(VectorPlaneTest, CrossUsesTheComplementaryAxisAndRightHandedOrientation) {
        using V = typename TypeParam::Vector;
        using CI = typename TypeParam::Info;
        const V left(2, 3), right(4, 5);
        const auto normal = left.cross(right), opposite = right.cross(left);
        using Normal = typename decltype(normal)::coordinate_info;
        static_assert(Normal::EnabledX == !CI::EnabledX);
        static_assert(Normal::EnabledY == !CI::EnabledY);
        static_assert(Normal::EnabledZ == !CI::EnabledZ);
        constexpr int expected = CI::EnabledX && CI::EnabledZ ? 2 : -2;
        if constexpr (Normal::EnabledX) {
            EXPECT_EQ(normal.value.x.value, expected);
            EXPECT_EQ(opposite.value.x.value, -expected);
        }
        if constexpr (Normal::EnabledY) {
            EXPECT_EQ(normal.value.y.value, expected);
            EXPECT_EQ(opposite.value.y.value, -expected);
        }
        if constexpr (Normal::EnabledZ) {
            EXPECT_EQ(normal.value.z.value, expected);
            EXPECT_EQ(opposite.value.z.value, -expected);
        }
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
    template<class CI>
    class VectorTwoDimensionalInputTest : public testing::Test {};
    using ProductPlanes = testing::Types<
        aho::coordinate::_MakeCoordinateInfo<true, true, false>::value,
        aho::coordinate::_MakeCoordinateInfo<true, false, true>::value,
        aho::coordinate::_MakeCoordinateInfo<false, true, true>::value>;
    TYPED_TEST_SUITE(VectorTwoDimensionalInputTest, ProductPlanes);
    TYPED_TEST(VectorTwoDimensionalInputTest, DerivedInputsPreservePlaneAndPromoteMixedProducts) {
        using CI = TypeParam;
        using Result = aho::_Vector<double, vsl::D2, CI>;
        using CrossCI = aho::coordinate::_MakeCoordinateInfo<!CI::EnabledX, !CI::EnabledY, !CI::EnabledZ>::value;
        using CrossResult = aho::_Vector<double, vsl::D1, CrossCI>;
        const aho::_Vector<int, vsl::D2, CI> left(2, 3);
        const aho::d2::Vector<double, CI> right(0.5, 1.5);
        const auto product = left.hadamard(right);
        const auto cross = left.cross(right);
        static_assert(std::same_as<std::remove_cv_t<decltype(product)>, Result>);
        static_assert(std::same_as<std::remove_cv_t<decltype(cross)>, CrossResult>);
        static_assert(std::same_as<decltype(left.dot(right)), double>);
        EXPECT_DOUBLE_EQ(left.dot(right), 5.5);
        EXPECT_DOUBLE_EQ(right.dot(left), 5.5);
        const auto check_components = [](const auto& value, double first, double second) {
            if constexpr (CI::EnabledX)
                EXPECT_DOUBLE_EQ(value.value.x.value, first);
            else
                EXPECT_DOUBLE_EQ(value.value.y.value, first);
            if constexpr (CI::EnabledZ)
                EXPECT_DOUBLE_EQ(value.value.z.value, second);
            else
                EXPECT_DOUBLE_EQ(value.value.y.value, second);
        };
        check_components(product, 1, 4.5);
        check_components(right.hadamard(left), 1, 4.5);
        const auto component = [](const auto& value) {
            if constexpr (CrossCI::EnabledX)
                return value.value.x.value;
            else if constexpr (CrossCI::EnabledY)
                return value.value.y.value;
            else
                return value.value.z.value;
        };
        constexpr double sign = CI::EnabledX && CI::EnabledZ ? -1 : 1;
        EXPECT_DOUBLE_EQ(component(cross), sign * 1.5);
        EXPECT_DOUBLE_EQ(component(right.cross(left)), -sign * 1.5);
        check_components(left, 2, 3);
        check_components(right, 0.5, 1.5);
    }
    TEST(VectorCastTest, FloatingPointToIntegerCastTruncatesTowardZeroInEveryDimension) {
        aho_test::expect_coordinates(aho::d1::VectorD(-2.75).cast<int>(), -2);
        aho_test::expect_coordinates(aho::d2::VectorD(-2.75, 3.75).cast<int>(), -2, 3);
        aho_test::expect_coordinates(aho::d3::VectorD(-2.75, 3.75, 4.5).cast<int>(), -2, 3, 4);
    }
    template<bool X, bool Y, bool Z>
    struct OneDimensionalInputCase {
        using Info = aho::coordinate::_MakeCoordinateInfo<X, Y, Z>::value;
    };
    using OneDimensionalInputAxes = testing::Types<OneDimensionalInputCase<true, false, false>,
                                                   OneDimensionalInputCase<false, true, false>, OneDimensionalInputCase<false, false, true>>;
    template<class T>
    class VectorOneDimensionalInputTest : public testing::Test {};
    TYPED_TEST_SUITE(VectorOneDimensionalInputTest, OneDimensionalInputAxes);
    TYPED_TEST(VectorOneDimensionalInputTest, ProductsAcceptBaseAndDerivedInputsOnTheSameAxis) {
        using CI = typename TypeParam::Info;
        using Left = aho::_Vector<double, vsl::D1, CI>;
        using Right = aho::_Vector<float, vsl::D1, CI>;
        const Left left(1), negative(-2);
        const Right base(4);
        const aho::d1::Vector<float, CI> derived(4);
        static_assert(std::same_as<decltype(left.dot(derived)), double>);
        static_assert(std::same_as<decltype(left.hadamard(derived)), Left>);
        auto coordinate = [](const auto& value) {
            if constexpr (CI::EnabledX)
                return value.value.x.value;
            else if constexpr (CI::EnabledY)
                return value.value.y.value;
            else
                return value.value.z.value;
        };
        EXPECT_EQ(left.dot(base), 4);
        EXPECT_EQ(left.dot(derived), 4);
        EXPECT_EQ(coordinate(left.hadamard(base)), 4);
        EXPECT_EQ(coordinate(left.hadamard(derived)), 4);
        EXPECT_EQ(negative.dot(derived), -8);
        EXPECT_EQ(coordinate(negative.hadamard(derived)), -8);
        EXPECT_EQ(coordinate(left), 1);
        EXPECT_EQ(coordinate(base), 4);
        EXPECT_EQ(coordinate(derived), 4);
    }
    template<class L, class R>
    concept HasOneDimensionalProducts = requires(const L& left, const R& right) {
        left.dot(right);
        left.hadamard(right);
    };
    using ProductAxisX = aho::coordinate::_MakeCoordinateInfo<true, false, false>::value;
    using ProductAxisY = aho::coordinate::_MakeCoordinateInfo<false, true, false>::value;
    static_assert(!HasOneDimensionalProducts<aho::_Vector<double, vsl::D1, ProductAxisX>, aho::_Vector<float, vsl::D1, ProductAxisY>>);
    static_assert(!HasOneDimensionalProducts<aho::d1::VectorD, aho::d2::VectorD>);
    static_assert(!HasOneDimensionalProducts<aho::d1::VectorD, aho::d1::PointD>);
    TEST(VectorConceptTest, UnrelatedTypesWithAliasesAreNotVectors) {
        struct PretendVector {
            using element_type = double;
            using dimension = vsl::D2;
        };
        EXPECT_FALSE(aho::concepts::is_vector<PretendVector>);
        EXPECT_TRUE(aho::concepts::is_vector<aho::d2::VectorD>);
        struct PretendWithCoordinates {
            using element_type = double;
            using dimension = vsl::D2;
            using coordinate_info = aho::coordinate::_DefaultCoordinateInfo<dimension>::value;
        };
        static_assert(!aho::concepts::is_vector<PretendVector>);
        static_assert(!aho::concepts::is_vector<PretendWithCoordinates>);
        static_assert(!aho::concepts::is_vector<int>);
        static_assert(!aho::concepts::is_vector<aho::d2::PointD>);
        static_assert(aho::concepts::is_vector<aho::d1::VectorI>);
        static_assert(aho::concepts::is_vector<aho::d2::VectorF>);
        static_assert(aho::concepts::is_vector<aho::d3::VectorD>);
        static_assert(aho::concepts::is_vector<aho::d2::Vector<double, aho::coordinate::_DefaultCoordinateInfo<vsl::D2>::value>>);
        static_assert(aho::concepts::is_vector<aho::d2::VectorD&>);
        static_assert(aho::concepts::is_vector<const aho::d2::VectorD&>);
        using XZ = aho::coordinate::_MakeCoordinateInfo<true, false, true>::value;
        static_assert(aho::concepts::is_vector<aho::_Vector<float, vsl::D2, XZ>>);
    }
    TYPED_TEST(VectorTest, MixedCompoundAssignmentsKeepTheLeftHandTypeAndReturnItsReference) {
        using V = typename TypeParam::Vector;
        using Mixed = aho::_Vector<long double, typename TypeParam::dimension>;
        auto value = aho_test::geometry<V>(2, 4, 6);
        const auto delta = aho_test::geometry<Mixed>(1, 2, 3);
        static_assert(std::same_as<decltype(value += delta), V&>);
        static_assert(std::same_as<decltype(value -= delta), V&>);
        static_assert(std::same_as<decltype(value *= 2.0L), V&>);
        static_assert(std::same_as<decltype(value /= 2.0L), V&>);
        EXPECT_EQ(&(value += delta), &value);
        aho_test::expect_coordinates(value, 3, 6, 9);
        EXPECT_EQ(&(value -= delta), &value);
        aho_test::expect_coordinates(value, 2, 4, 6);
        EXPECT_EQ(&(value *= 2.0L), &value);
        aho_test::expect_coordinates(value, 4, 8, 12);
        EXPECT_EQ(&(value /= 2.0L), &value);
        aho_test::expect_coordinates(value, 2, 4, 6);
        aho_test::expect_coordinates(delta, 1, 2, 3);
    }

} // namespace
#endif
