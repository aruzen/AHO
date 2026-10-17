#include <gtest/gtest.h>
#include <AHO/core/math/coordinate.hpp>
#include <sstream>

#if defined(AHO_CONTRACT_COORDINATE_ENABLE_AXES)
constexpr auto x = aho::coordinate::_MakeCoordinateInfo<false, true, false>::value::EnablingX();
constexpr auto y = aho::coordinate::_MakeCoordinateInfo<true, false, false>::value::EnablingY();
constexpr auto z = aho::coordinate::_MakeCoordinateInfo<true, false, false>::value::EnablingZ();

template<bool X, bool Y, bool Z>
consteval bool enabling_preserves_other_axes() {
    using Info = aho::coordinate::_MakeCoordinateInfo<X, Y, Z>::value;
    constexpr auto with_x = Info::EnablingX();
    constexpr auto with_y = Info::EnablingY();
    constexpr auto with_z = Info::EnablingZ();
    static_assert(with_x.EnabledX && with_x.EnabledY == Y && with_x.EnabledZ == Z);
    static_assert(with_y.EnabledX == X && with_y.EnabledY && with_y.EnabledZ == Z);
    static_assert(with_z.EnabledX == X && with_z.EnabledY == Y && with_z.EnabledZ);
    static_assert(decltype(with_x)::dimension::dimension == 1 + Y + Z);
    static_assert(decltype(with_y)::dimension::dimension == X + 1 + Z);
    static_assert(decltype(with_z)::dimension::dimension == X + Y + 1);
    return true;
}
static_assert(enabling_preserves_other_axes<true, false, false>());
static_assert(enabling_preserves_other_axes<false, true, false>());
static_assert(enabling_preserves_other_axes<false, false, true>());
static_assert(enabling_preserves_other_axes<true, true, false>());
static_assert(enabling_preserves_other_axes<true, false, true>());
static_assert(enabling_preserves_other_axes<false, true, true>());
static_assert(enabling_preserves_other_axes<true, true, true>());

#elif defined(AHO_CONTRACT_COORDINATE_YZ_ADD)
constexpr auto coordinates = aho::Y<int>(2) + aho::Z<int>(3);
static_assert(coordinates.y.value == 2 && coordinates.z.value == 3);
#elif defined(AHO_CONTRACT_COORDINATE_XZ_SUBTRACT)
constexpr auto coordinates = aho::X<int>(2) - aho::Z<int>(3);
static_assert(coordinates.x.value == 2 && coordinates.z.value == -3);
#elif defined(AHO_CONTRACT_COORDINATE_COMPARISON)
TEST(CoordinateContractTest, ThreeWayComparisonOrdersAndRecognizesEqualCoordinateSets) {
    const auto first = aho::X<int>(2) + aho::Y<int>(3);
    const auto second = aho::X<int>(3) + aho::Y<int>(4);
    EXPECT_TRUE(std::is_lt(first <=> second));
    EXPECT_TRUE(first == first);
}
#else
namespace {
template<bool X, bool Y, bool Z> struct CoordinateMask {
    static constexpr bool x = X, y = Y, z = Z;
    using Info = aho::coordinate::_MakeCoordinateInfo<X, Y, Z>::value;
    using Set = aho::coordinate::_CoordinateSet<int, Info>;
    static Set make() {
        if constexpr (X && Y && Z) return Set(aho::X<int>(1), aho::Y<int>(2), aho::Z<int>(3));
        else if constexpr (X && Y) return Set(aho::X<int>(1), aho::Y<int>(2));
        else if constexpr (X && Z) return Set(aho::X<int>(1), aho::Z<int>(3));
        else if constexpr (Y && Z) return Set(aho::Y<int>(2), aho::Z<int>(3));
        else if constexpr (X) return Set(aho::X<int>(1));
        else if constexpr (Y) return Set(aho::Y<int>(2));
        else return Set(aho::Z<int>(3));
    }
};
using CoordinateMasks = testing::Types<CoordinateMask<true, false, false>, CoordinateMask<false, true, false>,
    CoordinateMask<false, false, true>, CoordinateMask<true, true, false>, CoordinateMask<true, false, true>,
    CoordinateMask<false, true, true>, CoordinateMask<true, true, true>>;
template<class T> class CoordinateMaskTest : public testing::Test {};
TYPED_TEST_SUITE(CoordinateMaskTest, CoordinateMasks);
TYPED_TEST(CoordinateMaskTest, DefaultAndAxisConstructorsInitializeOnlyEnabledAxes) {
    typename TypeParam::Set zero;
    const auto value = TypeParam::make();
    if constexpr (TypeParam::x) { EXPECT_EQ(zero.x.value, 0); EXPECT_EQ(value.x.value, 1); }
    if constexpr (TypeParam::y) { EXPECT_EQ(zero.y.value, 0); EXPECT_EQ(value.y.value, 2); }
    if constexpr (TypeParam::z) { EXPECT_EQ(zero.z.value, 0); EXPECT_EQ(value.z.value, 3); }
    EXPECT_TRUE(zero < value); EXPECT_FALSE(value < value);
    EXPECT_TRUE(aho::concepts::is_coordinate_set<typename TypeParam::Set>);
}
TYPED_TEST(CoordinateMaskTest, AxisCountsPreserveEveryEnabledPrefix) {
    using Info = typename TypeParam::Info;
    static_assert(std::same_as<decltype(Info::IndexX), const std::size_t>);
    static_assert(std::same_as<decltype(Info::IndexY), const std::size_t>);
    static_assert(std::same_as<decltype(Info::IndexZ), const std::size_t>);
    EXPECT_EQ(Info::IndexX, TypeParam::x ? 1u : 0u);
    EXPECT_EQ(Info::IndexY, static_cast<unsigned>(TypeParam::x) + TypeParam::y);
    EXPECT_EQ(Info::IndexZ, static_cast<unsigned>(TypeParam::x) + TypeParam::y + TypeParam::z);
}
struct UnmarkedCoordinateType {};
struct DisabledCoordinateType { static constexpr bool is_coordinate_set = false; };
static_assert(!aho::concepts::is_coordinate_set<int>);
static_assert(!aho::concepts::is_coordinate_set<UnmarkedCoordinateType>);
static_assert(!aho::concepts::is_coordinate_set<DisabledCoordinateType>);
TYPED_TEST(CoordinateMaskTest, AxisAssignmentOperatorsReturnAndMutateOriginalSet) {
    auto value = TypeParam::make();
    if constexpr (TypeParam::x) {
        EXPECT_EQ(&(value += aho::X<int>(2)), &value); EXPECT_EQ(value.x.value, 3);
        EXPECT_EQ(&(value -= aho::X<int>(1)), &value); EXPECT_EQ(value.x.value, 2);
    }
    if constexpr (TypeParam::y) {
        EXPECT_EQ(&(value += aho::Y<int>(2)), &value); EXPECT_EQ(value.y.value, 4);
        EXPECT_EQ(&(value -= aho::Y<int>(1)), &value); EXPECT_EQ(value.y.value, 3);
    }
    if constexpr (TypeParam::z) {
        EXPECT_EQ(&(value += aho::Z<int>(2)), &value); EXPECT_EQ(value.z.value, 5);
        EXPECT_EQ(&(value -= aho::Z<int>(1)), &value); EXPECT_EQ(value.z.value, 4);
    }
}
TYPED_TEST(CoordinateMaskTest, StreamOutputIncludesAllEnabledAxesAndCoordinates) {
    std::ostringstream stream;
    aho::operator<<(stream, TypeParam::make());
    if constexpr (TypeParam::x) { EXPECT_NE(stream.str().find('x'), std::string::npos); EXPECT_NE(stream.str().find('1'), std::string::npos); }
    if constexpr (TypeParam::y) { EXPECT_NE(stream.str().find('y'), std::string::npos); EXPECT_NE(stream.str().find('2'), std::string::npos); }
    if constexpr (TypeParam::z) { EXPECT_NE(stream.str().find('z'), std::string::npos); EXPECT_NE(stream.str().find('3'), std::string::npos); }
}
template<class T> class CoordinateElementTest : public testing::Test {};
using CoordinateElements = testing::Types<aho::X<double>, aho::Y<double>, aho::Z<double>>;
TYPED_TEST_SUITE(CoordinateElementTest, CoordinateElements);
TYPED_TEST(CoordinateElementTest, ConstructorsUnaryBinaryAndComparisonOperatorsPreserveValues) {
    const TypeParam a(12), b(3);
    EXPECT_EQ(TypeParam{}.value, 0);
    EXPECT_EQ((+a).value, 12); EXPECT_EQ((-a).value, -12);
    EXPECT_EQ((a + b).value, 15); EXPECT_EQ((a - b).value, 9);
    EXPECT_EQ((a * b).value, 36); EXPECT_EQ((a / b).value, 4);
    EXPECT_TRUE(a > b); EXPECT_TRUE(a == a);
    EXPECT_EQ(a.template cast<int>().value, 12);
}
TYPED_TEST(CoordinateElementTest, AssignmentsMutateAndReturnOriginalObject) {
    TypeParam a(12), b(3);
    EXPECT_EQ(&(a += b), &a); EXPECT_EQ(a.value, 15);
    EXPECT_EQ(&(a -= b), &a); EXPECT_EQ(a.value, 12);
    EXPECT_EQ(&(a *= b), &a); EXPECT_EQ(a.value, 36);
    EXPECT_EQ(&(a /= b), &a); EXPECT_EQ(a.value, 12);
}
TYPED_TEST(CoordinateElementTest, OutputNamesTheAxisAndValue) {
    std::ostringstream stream;
    stream << TypeParam(12);
    EXPECT_EQ(stream.str(), std::string(1, TypeParam::LowerChar) + " : 12");
}
TEST(CoordinateInfoTest, EnabledFlagsDimensionAndAxisCountsMatchConfiguration) {
    using Info = aho::coordinate::_MakeCoordinateInfo<true, true, true>::value;
    static_assert(std::same_as<Info::dimension, vsl::D3>);
    EXPECT_TRUE(Info::EnabledX); EXPECT_TRUE(Info::EnabledY); EXPECT_TRUE(Info::EnabledZ);
    EXPECT_EQ(Info::IndexX, 1); EXPECT_EQ(Info::IndexY, 2); EXPECT_EQ(Info::IndexZ, 3);
    EXPECT_EQ((aho::coordinate::_CountTruesFunc<true, false, true>()), 2u);
    EXPECT_EQ((aho::coordinate::_CountTrues<false, false, false>::value), 0u);
}
TEST(CoordinateSetTest, ConstructorsAndAxisAssignmentsKeepNamedValues) {
    using CI = aho::coordinate::_DefaultCoordinateInfo<vsl::D3>::value;
    aho::coordinate::_CoordinateSet<int, CI> value(1, 2, 3), zero;
    EXPECT_EQ(zero.x.value, 0); EXPECT_EQ(zero.y.value, 0); EXPECT_EQ(zero.z.value, 0);
    EXPECT_EQ(&(value += aho::X<int>(2)), &value); EXPECT_EQ(value.x.value, 3);
    value -= aho::X<int>(1); value += aho::Y<int>(2); value -= aho::Y<int>(1); value += aho::Z<int>(2); value -= aho::Z<int>(1);
    EXPECT_EQ(value.x.value, 2); EXPECT_EQ(value.y.value, 3); EXPECT_EQ(value.z.value, 4);
    EXPECT_TRUE(zero < value);
}
TEST(CoordinateSetTest, AdditionAndSubtractionCombineMatchingAndDifferentAxes) {
    const auto xy = aho::X<int>(2) + aho::Y<int>(3);
    EXPECT_EQ(xy.x.value, 2); EXPECT_EQ(xy.y.value, 3);
    const auto xyz = xy + aho::Z<int>(4);
    EXPECT_EQ(xyz.x.value, 2); EXPECT_EQ(xyz.y.value, 3); EXPECT_EQ(xyz.z.value, 4);
    const auto sum = xyz + xyz;
    EXPECT_EQ(sum.x.value, 4); EXPECT_EQ(sum.y.value, 6); EXPECT_EQ(sum.z.value, 8);
    const auto difference = sum - xyz;
    EXPECT_EQ(difference.x.value, 2); EXPECT_EQ(difference.y.value, 3); EXPECT_EQ(difference.z.value, 4);
    const auto moved = xyz - aho::X<int>(1);
    EXPECT_EQ(moved.x.value, 1); EXPECT_EQ(moved.y.value, 3); EXPECT_EQ(moved.z.value, 4);
}
template<class T> class CoordinateAxisPairTest : public testing::Test {};
using PairScalars = testing::Types<int, float, double>;
TYPED_TEST_SUITE(CoordinateAxisPairTest, PairScalars);
TYPED_TEST(CoordinateAxisPairTest, AdditionUsesTheAxesOfBothOperands) {
    using T = TypeParam;
    constexpr auto xy = aho::X<T>(2) + aho::Y<T>(-3);
    constexpr auto xz = aho::X<T>(2) + aho::Z<T>(-4);
    constexpr auto yz = aho::Y<T>(-3) + aho::Z<T>(-4);
    static_assert(xy.x.value == 2 && xy.y.value == -3);
    static_assert(xz.x.value == 2 && xz.z.value == -4);
    static_assert(yz.y.value == -3 && yz.z.value == -4);
    EXPECT_EQ(xy.x.value, 2); EXPECT_EQ(xy.y.value, -3);
    EXPECT_EQ(xz.x.value, 2); EXPECT_EQ(xz.z.value, -4);
    EXPECT_EQ(yz.y.value, -3); EXPECT_EQ(yz.z.value, -4);
}
TYPED_TEST(CoordinateAxisPairTest, SubtractionPreservesLeftAxisAndNegatesRightAxisInBothOrders) {
    using T = TypeParam;
    constexpr auto xy = aho::X<T>(2) - aho::Y<T>(3);
    constexpr auto yx = aho::Y<T>(3) - aho::X<T>(2);
    constexpr auto xz = aho::X<T>(2) - aho::Z<T>(4);
    constexpr auto zx = aho::Z<T>(4) - aho::X<T>(2);
    constexpr auto yz = aho::Y<T>(3) - aho::Z<T>(4);
    constexpr auto zy = aho::Z<T>(4) - aho::Y<T>(3);
    static_assert(xy.x.value == 2 && xy.y.value == -3);
    static_assert(yx.x.value == -2 && yx.y.value == 3);
    static_assert(xz.x.value == 2 && xz.z.value == -4);
    static_assert(zx.x.value == -2 && zx.z.value == 4);
    static_assert(yz.y.value == 3 && yz.z.value == -4);
    static_assert(zy.y.value == -3 && zy.z.value == 4);
    EXPECT_EQ(xy.x.value, 2); EXPECT_EQ(xy.y.value, -3);
    EXPECT_EQ(yx.x.value, -2); EXPECT_EQ(yx.y.value, 3);
    EXPECT_EQ(xz.x.value, 2); EXPECT_EQ(xz.z.value, -4);
    EXPECT_EQ(zx.x.value, -2); EXPECT_EQ(zx.z.value, 4);
    EXPECT_EQ(yz.y.value, 3); EXPECT_EQ(yz.z.value, -4);
    EXPECT_EQ(zy.y.value, -3); EXPECT_EQ(zy.z.value, 4);
}
TEST(CoordinateMixedAxisPairTest, AdditionPromotesBothComponentsForEveryPlane) {
    constexpr auto xy = aho::X<int>(2) + aho::Y<double>(-3.5);
    constexpr auto xz = aho::X<float>(2.5f) + aho::Z<int>(-4);
    constexpr auto yz = aho::Y<unsigned>(3) + aho::Z<double>(-4.5);
    static_assert(std::same_as<typename decltype(xy)::element_type, double>);
    static_assert(std::same_as<typename decltype(xz)::element_type, float>);
    static_assert(std::same_as<typename decltype(yz)::element_type, double>);
    static_assert(xy.x.value == 2 && xy.y.value == -3.5);
    static_assert(xz.x.value == 2.5 && xz.z.value == -4);
    static_assert(yz.y.value == 3 && yz.z.value == -4.5);
    EXPECT_EQ(xy.y.value, -3.5);
    EXPECT_EQ(xz.x.value, 2.5);
    EXPECT_EQ(yz.z.value, -4.5);
}
TEST(CoordinateMixedAxisPairTest, SubtractionPromotesBeforeNegatingUnsignedComponentsInEveryOrder) {
    constexpr auto xy = aho::X<double>(2.5) - aho::Y<unsigned>(3);
    constexpr auto yx = aho::Y<float>(3.5f) - aho::X<unsigned>(2);
    constexpr auto xz = aho::X<int>(2) - aho::Z<double>(4.5);
    constexpr auto zx = aho::Z<double>(4.5) - aho::X<unsigned>(2);
    constexpr auto yz = aho::Y<double>(3.5) - aho::Z<unsigned>(4);
    constexpr auto zy = aho::Z<float>(4.5f) - aho::Y<unsigned>(3);
    static_assert(xy.x.value == 2.5 && xy.y.value == -3);
    static_assert(yx.x.value == -2 && yx.y.value == 3.5);
    static_assert(xz.x.value == 2 && xz.z.value == -4.5);
    static_assert(zx.x.value == -2 && zx.z.value == 4.5);
    static_assert(yz.y.value == 3.5 && yz.z.value == -4);
    static_assert(zy.y.value == -3 && zy.z.value == 4.5);
    static_assert(std::same_as<typename decltype(xy)::element_type, double>);
    static_assert(std::same_as<typename decltype(yx)::element_type, float>);
    EXPECT_EQ(xy.y.value, -3); EXPECT_EQ(yx.x.value, -2);
    EXPECT_EQ(xz.z.value, -4.5); EXPECT_EQ(zx.x.value, -2);
    EXPECT_EQ(yz.z.value, -4); EXPECT_EQ(zy.y.value, -3);
}
TEST(CoordinateLiteralTest, AllAxisLiteralTypesPreserveValues) {
    using namespace aho::literals;
    EXPECT_EQ((2_x).value, 2); EXPECT_EQ((3_y).value, 3); EXPECT_EQ((4_z).value, 4);
    EXPECT_EQ((0.5_x).value, 0.5); EXPECT_EQ((0.5_y).value, 0.5); EXPECT_EQ((0.5_z).value, 0.5);
    EXPECT_EQ((0.5_f_x).value, 0.5f); EXPECT_EQ((0.5_f_y).value, 0.5f); EXPECT_EQ((0.5_f_z).value, 0.5f);
    EXPECT_EQ((2_l_x).value, 2u); EXPECT_EQ((3_l_y).value, 3u); EXPECT_EQ((4_l_z).value, 4u);
}
}
#endif
