#include <gtest/gtest.h>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>
#include <AHO/engine.hpp>
namespace {
TEST(EngineAccessorTest, FirstAccessorBecomesSingletonAndLaterAccessorsDoNotReplaceIt) {
    const auto previous = aho::engine::EngineAccessor::instance();
    aho::engine::EngineAccessor first;
    aho::engine::EngineAccessor second;
    EXPECT_EQ(aho::engine::EngineAccessor::instance(), previous ? previous : &first);
    EXPECT_EQ(first._data, nullptr);
    EXPECT_EQ(second._data, nullptr);
}
TEST(EngineAccessorLifetimeTest, CopiesShareDataAndMovesKeepTransferringOwnership) {
    using Accessor = aho::engine::EngineAccessor;
    static_assert(std::is_nothrow_move_constructible_v<Accessor>);
    static_assert(std::is_nothrow_move_assignable_v<Accessor>);
    // Own a control block without constructing GPU-backed EngineData.
    auto token = std::make_shared<int>(7);
    Accessor source;
    source._data = std::shared_ptr<aho::engine::EngineData>(token, nullptr);
    const auto registered = Accessor::instance();
    Accessor copy(source);
    EXPECT_EQ(token.use_count(), 3);
    Accessor moved(std::move(copy));
    EXPECT_EQ(copy._data.use_count(), 0);
    EXPECT_EQ(moved._data.use_count(), 3);
    Accessor assigned;
    assigned = source;
    EXPECT_EQ(token.use_count(), 4);
    Accessor move_assigned;
    move_assigned = std::move(assigned);
    EXPECT_EQ(assigned._data.use_count(), 0);
    EXPECT_EQ(move_assigned._data.use_count(), 4);
    EXPECT_EQ(Accessor::instance(), registered);
}
TEST(EngineAccessorLifetimeTest, DestroyingTheRegisteredAccessorClearsItAndAllowsTheNextOwner) {
    const auto previous_style = GTEST_FLAG_GET(death_test_style);
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    // Re-exec this case so other tests' static accessors cannot affect the registry.
    EXPECT_EXIT({
        using Accessor = aho::engine::EngineAccessor;
        if (Accessor::instance() != nullptr) std::_Exit(1);
        {
            Accessor first;
            if (Accessor::instance() != &first) std::_Exit(2);
            { Accessor later; }
            if (Accessor::instance() != &first) std::_Exit(3);
        }
        if (Accessor::instance() != nullptr) std::_Exit(4);
        {
            Accessor next;
            if (Accessor::instance() != &next) std::_Exit(5);
        }
        if (Accessor::instance() != nullptr) std::_Exit(6);
        std::_Exit(0);
    }, testing::ExitedWithCode(0), "");
    GTEST_FLAG_SET(death_test_style, previous_style);
}

}
