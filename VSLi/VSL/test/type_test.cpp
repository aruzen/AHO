#include <VSL/type.hpp>
#include <gtest/gtest.h>
#include <compare>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {
    struct Kind : vsl::type<Kind> {
        consteval Kind(const char *name, char index) : type(name, index) {}
    };
    TEST(TypeTest, ConstTagsExposeTheirNameAndIndex) {
        constexpr Kind kind("vertex", 7);
        static_assert(kind.index() == 7);
        static_assert(kind.name()[0] == 'v');
        EXPECT_STREQ(kind.name(), "vertex");
        EXPECT_EQ(kind.index(), 7);
    }
    TEST(TypeTest, ComparisonRetainsTheExistingDescendingIndexOrder) {
        constexpr Kind first("first", 1), second("second", 2);
        static_assert(std::same_as<decltype(first <=> second), std::strong_ordering>);
        static_assert(std::is_gt(first <=> second));
        static_assert(std::is_lt(second <=> first));
        static_assert(std::is_eq(first <=> first));
        EXPECT_GT(first, second);
        EXPECT_LT(second, first);
        EXPECT_TRUE(first >= second);
        EXPECT_TRUE(second <= first);
        auto mutable_first = first;
        auto mutable_second = second;
        EXPECT_EQ(mutable_first <=> mutable_second, std::strong_ordering::greater);
    }
    TEST(TypeTest, EqualIndicesCompareEquivalentRegardlessOfName) {
        constexpr Kind first("first", 3), alias("alias", 3);
        static_assert(std::is_eq(first <=> alias));
        EXPECT_EQ(first <=> alias, std::strong_ordering::equal);
        EXPECT_TRUE(first <= alias);
        EXPECT_TRUE(first >= alias);
    }
    TEST(TypeTest, SignedIndexExtremesDoNotOverflowDuringComparison) {
        struct WideKind : vsl::type<WideKind, std::int64_t> {
            consteval WideKind(std::int64_t value) : type("wide", value) {}
        };
        constexpr WideKind low(std::numeric_limits<std::int64_t>::min());
        constexpr WideKind high(std::numeric_limits<std::int64_t>::max());
        static_assert(std::is_gt(low <=> high));
        EXPECT_EQ(high <=> low, std::strong_ordering::less);
    }
    TEST(TypeTest, FloatingPointIndicesKeepPartialOrderingAndNaNs) {
        struct FloatingKind : vsl::type<FloatingKind, double> {
            consteval FloatingKind(double value) : type("floating", value) {}
        };
        constexpr FloatingKind first(1), second(2), nan(std::numeric_limits<double>::quiet_NaN());
        static_assert(std::same_as<decltype(first <=> second), std::partial_ordering>);
        static_assert(std::is_gt(first <=> second));
        EXPECT_EQ(first <=> nan, std::partial_ordering::unordered);
        EXPECT_EQ(nan <=> first, std::partial_ordering::unordered);
        EXPECT_FALSE(first < nan);
        EXPECT_FALSE(first > nan);
    }
} // namespace
