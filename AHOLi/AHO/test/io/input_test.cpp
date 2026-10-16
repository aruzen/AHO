#include <AHO/io/input.hpp>
#if defined(AHO_CONTRACT_INPUT_MOUSE_INTEGER)
void verify() { aho::input::MouseID id(0); }
#elif defined(AHO_LINK_INPUT_KEY_NAME)
int main() { return aho::input::KeyID(aho::input::KeyCode::A).name().empty(); }
#elif defined(AHO_LINK_INPUT_MOUSE_NAME)
int main() { return aho::input::MouseID(aho::input::MouseCode::LeftClick).name() != "LeftClick"; }
#else
#include <gtest/gtest.h>
#include <set>
namespace {
TEST(InputInterfaceTest, DefaultUpdatePreservesStateAndDoesNotThrow) {
    struct DefaultInput : aho::input::abstract::Input<int>, aho::input::abstract::Updatable {
        int value = 7;
        int state() override { return value; }
    } input;
    EXPECT_NO_THROW(input.update());
    EXPECT_EQ(input.state(), 7);
}
TEST(InputCodeTest, TypedAndIntegerKeyConstructorsPreserveCodes) {
    const aho::input::KeyID typed(aho::input::KeyCode::A), integer(65);
    EXPECT_EQ(typed.code(), aho::input::KeyCode::A);
    EXPECT_EQ(integer.code(), aho::input::KeyCode::A);
    EXPECT_EQ(static_cast<int>(aho::input::KeyID(-1).code()), -1);
    EXPECT_EQ(aho::input::MouseID(aho::input::MouseCode::RightClick).code(), aho::input::MouseCode::Button2);
}
TEST(InputCodeTest, EnumeratedKeysAndMouseButtonsAreUniqueAndComplete) {
    using namespace aho::input;
    const std::set<KeyCode> keys(KeyCodes.begin(), KeyCodes.end());
    EXPECT_EQ(keys.size(), KeyCodes.size());
    EXPECT_TRUE(keys.contains(KeyCode::A)); EXPECT_TRUE(keys.contains(KeyCode::F25)); EXPECT_TRUE(keys.contains(KeyCode::Menu));
    const std::set<MouseCode> buttons(MouseCodes.begin(), MouseCodes.end());
    EXPECT_EQ(buttons.size(), 8u);
    EXPECT_EQ(MouseCode::LeftClick, MouseCode::Button1);
    EXPECT_EQ(MouseCode::RightClick, MouseCode::Button2);
    EXPECT_EQ(MouseCode::LastButton, MouseCode::Button8);
}
}
#endif
