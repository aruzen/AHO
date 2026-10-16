#include <gtest/gtest.h>
#include <VSL/type.hpp>

namespace {
struct Kind : vsl::type<Kind> {
    consteval Kind(const char *name, char index) : type(name, index) {}
};
TEST(TypeTest, StoresNameAndIndexWithoutChangingThem) {
    Kind kind("vertex", 7);
    EXPECT_STREQ(kind.name(), "vertex");
    EXPECT_EQ(kind.index(), 7);
}
// 比較のコンパイル不具合 I26 は下のコンパイル検証で記録する。
} // namespace
#include "support/compile_probe.hpp"
namespace {
TEST(TypeTest, DISABLED_ComparisonReturnsAnOrderingCompatibleWithItsDeclaration) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <compare>
#include <VSL/type.hpp>
struct Kind:vsl::type<Kind>{consteval Kind(char index):type("kind",index){}};
auto probe(){return Kind(1)<=>Kind(2);}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
} // namespace
