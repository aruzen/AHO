#include <gtest/gtest.h>
#include <AHO/engine.hpp>
namespace {
TEST(EngineAccessorTest, FirstAccessorBecomesSingletonAndLaterAccessorsDoNotReplaceIt) {
    const auto previous = aho::engine::EngineAccessor::instance();
    static aho::engine::EngineAccessor first;
    static aho::engine::EngineAccessor second;
    EXPECT_EQ(aho::engine::EngineAccessor::instance(), previous ? previous : &first);
    EXPECT_EQ(first._data, nullptr);
    EXPECT_EQ(second._data, nullptr);
}
}
