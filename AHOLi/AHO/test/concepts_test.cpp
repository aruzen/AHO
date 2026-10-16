#include <gtest/gtest.h>
#include <AHO/concepts.hpp>
#include <tuple>

namespace {
struct Empty {};
struct Shape { double area() const { return 2; } };
struct Intersecting { bool intersect(Shape) { return true; } bool cotains(Shape) { return false; } };
static_assert(aho::concepts::is_std_tuple<std::tuple<int>&>);
static_assert(!aho::concepts::is_std_tuple<int>);
static_assert(aho::concepts::plusable<int> && aho::concepts::minusable<int>);
static_assert(!aho::concepts::plusable<Empty> && !aho::concepts::minusable<Empty>);
static_assert(aho::concepts::add_as<int, double> && aho::concepts::subtract_as<int, double>);
static_assert(aho::concepts::multiply_as<int, double> && aho::concepts::division_as<int, double>);
static_assert(!aho::concepts::add_as<Empty, int> && !aho::concepts::subtract_as<Empty, int>);
static_assert(!aho::concepts::multiply_as<Empty, int> && !aho::concepts::division_as<Empty, int>);
static_assert(aho::concepts::cast_as<int, double> && !aho::concepts::cast_as<Empty, int>);
static_assert(aho::concepts::sames_as<int> && aho::concepts::sames_as<int, int>);
static_assert(aho::concepts::sames_as<int, int, int> && !aho::concepts::sames_as<int, double>);
static_assert(aho::concepts::is_standard_object<Shape> && !aho::concepts::is_standard_object<Empty>);
static_assert(aho::concepts::is_intersect_object<Intersecting, Shape>);
TEST(AhoConceptsTest, AcceptedShapeCanBeUsedThroughItsPublicContract) {
    EXPECT_DOUBLE_EQ(Shape{}.area(), 2);
    EXPECT_TRUE(Intersecting{}.intersect(Shape{}));
    EXPECT_FALSE(Intersecting{}.cotains(Shape{}));
}
}
