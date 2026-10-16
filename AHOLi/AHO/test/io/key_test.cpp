#include <AHO/io/key.hpp>
#include "../support/input_backend.hpp"

namespace {
using KeyTest = aho_test::InputBackendTest;
TEST_F(KeyTest, BothConstructorsPreserveCodeAndStartIdle) {
    aho::InputManager manager(window.handle());
    const auto a = manager.get<aho::input::Key>(aho::input::KeyCode::A);
    const auto b = manager.get<aho::input::Key>(aho::input::KeyID(aho::input::KeyCode::B));
    EXPECT_EQ(a->code().code(), aho::input::KeyCode::A);
    EXPECT_EQ(b->code().code(), aho::input::KeyCode::B);
    EXPECT_EQ(a->state(), aho::input::ButtonState::Idle);
    EXPECT_FALSE(a->down()); EXPECT_FALSE(a->up()); EXPECT_FALSE(a->pressed());
}
TEST_F(KeyTest, KeyTransitionsThroughDownPressedUpAndIdle) {
    using namespace aho::input;
    aho::InputManager manager(window.handle());
    const auto key = manager.get<Key>(KeyCode::A);
    window.keys[GLFW_KEY_A] = GLFW_PRESS;
    key->update();
    EXPECT_EQ(key->state(), ButtonState::Down); EXPECT_TRUE(key->down()); EXPECT_TRUE(key->pressed()); EXPECT_FALSE(key->up());
    manager.update();
    EXPECT_EQ(key->state(), ButtonState::Pressed); EXPECT_FALSE(key->down()); EXPECT_TRUE(key->pressed());
    window.keys[GLFW_KEY_A] = GLFW_RELEASE;
    key->update();
    EXPECT_EQ(key->state(), ButtonState::Up); EXPECT_TRUE(key->up()); EXPECT_FALSE(key->pressed());
    key->update();
    EXPECT_EQ(key->state(), ButtonState::Idle); EXPECT_FALSE(key->up());
}
TEST_F(KeyTest, GroupedKeysKeepOrderAndRegisterEachKeyForUpdates) {
    using namespace aho::input;
    aho::InputManager manager(window.handle());
    const auto keys = manager.get<Keys>(KeyCode::A, KeyCode::B);
    ASSERT_EQ(keys->keys.size(), 2u);
    EXPECT_EQ(keys->keys[0]->code().code(), KeyCode::A);
    EXPECT_EQ(keys->keys[1]->code().code(), KeyCode::B);
    EXPECT_EQ(manager.update_targets.size(), 2u);
    window.keys[GLFW_KEY_B] = GLFW_PRESS;
    manager.update();
    EXPECT_EQ(keys->keys[0]->state(), ButtonState::Idle);
    EXPECT_EQ(keys->keys[1]->state(), ButtonState::Down);
}
}
