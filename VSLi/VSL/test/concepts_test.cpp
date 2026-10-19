#include <gtest/gtest.h>
#include <VSL/concepts.hpp>

namespace {
    struct Searcher {
        int search(double) {
            return 7;
        }
    };
    struct Checker {
        bool check(int v) {
            return v == 7;
        }
    };
    struct Creator {
        int create(double v) {
            return static_cast<int>(v);
        }
    };
    struct ExplicitOnly {
        explicit ExplicitOnly(int) {}
    };
    struct WrongResult {
        double check(int) {
            return 1.0;
        }
    };
    static_assert(vsl::concepts::searcher<double, int, Searcher>);
    static_assert(!vsl::concepts::searcher<double, bool, Searcher>);
    static_assert(vsl::concepts::checker<int, Checker>);
    static_assert(!vsl::concepts::checker<int, WrongResult>);
    static_assert(vsl::concepts::creator<double, int, Creator>);
    static_assert(!vsl::concepts::creator<double, bool, Creator>);
    static_assert(vsl::concepts::initializer<ExplicitOnly, int>);
    static_assert(!vsl::concepts::initializer<ExplicitOnly>);
    TEST(ConceptsTest, AcceptedOperationsProduceExpectedResults) {
        EXPECT_EQ(Searcher{}.search(1.0), 7);
        EXPECT_TRUE(Checker{}.check(7));
        EXPECT_EQ(Creator{}.create(3.5), 3);
    }
} // namespace
