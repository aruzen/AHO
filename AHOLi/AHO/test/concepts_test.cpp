#include <gtest/gtest.h>
#include <AHO/concepts.hpp>
#include <tuple>

namespace {
    struct Empty {};
    struct Shape {
        double area() const { return 2; }
    };
    struct Intersecting {
        bool intersect(Shape) { return true; }
        bool contains(Shape) { return false; }
    };
    struct OnlyIntersects {
        bool intersect(Shape) { return true; }
    };
    struct OnlyContains {
        bool contains(Shape) { return true; }
    };
    struct NonBooleanContains {
        bool intersect(Shape) { return true; }
        void contains(Shape) {}
    };
    struct NonBooleanIntersection {
        void intersect(Shape) {}
        bool contains(Shape) { return true; }
    };
    static_assert(!aho::concepts::is_intersect_object<OnlyIntersects, Shape>);
    static_assert(!aho::concepts::is_intersect_object<OnlyContains, Shape>);
    static_assert(!aho::concepts::is_intersect_object<NonBooleanContains, Shape>);
    static_assert(!aho::concepts::is_intersect_object<NonBooleanIntersection, Shape>);
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
    using aho::concepts::helper::GetTypeV;
    static_assert(std::same_as<GetTypeV<0, int, float, double>, int>);
    static_assert(std::same_as<GetTypeV<1, int, float, double>, float>);
    static_assert(std::same_as<GetTypeV<2, int, float, double>, double>);
    static_assert(std::same_as<GetTypeV<3, int, float, double, Empty>, Empty>);
    static_assert(std::same_as<GetTypeV<1, int, const Shape&>, const Shape&>);
    static_assert(std::same_as<GetTypeV<2, int, double, Shape&&>, Shape&&>);
    TEST(AHOConceptsTest, IntersectionConstraintRequiresBothBooleanOperations) {
        EXPECT_TRUE((aho::concepts::is_intersect_object<Intersecting, Shape>));
        EXPECT_FALSE((aho::concepts::is_intersect_object<OnlyIntersects, Shape>));
        EXPECT_FALSE((aho::concepts::is_intersect_object<OnlyContains, Shape>));
        EXPECT_FALSE((aho::concepts::is_intersect_object<NonBooleanContains, Shape>));
        EXPECT_FALSE((aho::concepts::is_intersect_object<NonBooleanIntersection, Shape>));
    }
    TEST(AHOConceptsTest, AcceptedShapeCanBeUsedThroughItsPublicContract) {
        EXPECT_DOUBLE_EQ(Shape{}.area(), 2);
        EXPECT_TRUE(Intersecting{}.intersect(Shape{}));
        EXPECT_FALSE(Intersecting{}.contains(Shape{}));
    }
} // namespace
