#include <AHO/io/keyboard.hpp>
#include "../support/input_backend.hpp"
#include <optional>
#include <type_traits>

extern std::unordered_set<aho::input::KeyCode> PRESSED_KEYS;
namespace {
using KeyboardTest = aho_test::InputBackendTest;
static_assert(std::is_nothrow_copy_constructible_v<aho::input::KeyBoard>);
TEST_F(KeyboardTest, CallbackUpdatesPressedKeysAndReleasesCallbackOnDestruction) {
    using namespace aho::input;
    const auto previous = PRESSED_KEYS;
    struct Restore { std::unordered_set<KeyCode> value; ~Restore() { PRESSED_KEYS = value; } } restore{previous};
    PRESSED_KEYS.clear();
    aho::InputManager manager(window.handle());
    {
        const auto keyboard = manager.get<KeyBoard>();
        ASSERT_NE(window.key_callback, nullptr);
        window.key_callback(window.handle(), GLFW_KEY_A, 0, GLFW_PRESS, 0);
        EXPECT_TRUE(keyboard->pressed(KeyCode::A));
        EXPECT_EQ(keyboard->state(), (std::unordered_set<KeyCode>{KeyCode::A}));
        window.key_callback(window.handle(), GLFW_KEY_A, 0, GLFW_REPEAT, 0);
        EXPECT_EQ(keyboard->state().size(), 1u);
        window.key_callback(window.handle(), GLFW_KEY_A, 0, GLFW_RELEASE, 0);
        EXPECT_FALSE(keyboard->pressed(KeyCode::A));
        EXPECT_TRUE(keyboard->state().empty());
    }
    EXPECT_EQ(window.key_callback, nullptr);
}
TEST_F(KeyboardTest, CallbackRemainsUntilLastKeyboardForWindowIsDestroyed) {
    using namespace aho::input;
    const auto previous = PRESSED_KEYS;
    struct Restore {
        std::unordered_set<KeyCode> value;
        ~Restore() { PRESSED_KEYS = value; }
    } restore{previous};
    aho::InputManager manager(window.handle());
    auto first = manager.get<KeyBoard>();
    auto second = manager.get<KeyBoard>();
    const auto callback = window.key_callback;
    ASSERT_NE(callback, nullptr);
    first.reset();
    EXPECT_EQ(window.key_callback, callback);
    callback(window.handle(), GLFW_KEY_B, 0, GLFW_PRESS, 0);
    EXPECT_TRUE(second->pressed(KeyCode::B));
    second.reset();
    EXPECT_EQ(window.key_callback, nullptr);
    EXPECT_TRUE(PRESSED_KEYS.empty());
}
TEST_F(KeyboardTest, CopiedKeyboardKeepsCallbackUntilBothObjectsAreDestroyed) {
    using namespace aho::input;
    const auto previous = PRESSED_KEYS;
    struct Restore {
        std::unordered_set<KeyCode> value;
        ~Restore() { PRESSED_KEYS = value; }
    } restore{previous};
    aho::InputManager manager(window.handle());
    auto original = manager.get<KeyBoard>();
    std::optional<KeyBoard> copy(std::in_place, *original);
    original.reset();
    ASSERT_NE(window.key_callback, nullptr);
    window.key_callback(window.handle(), GLFW_KEY_A, 0, GLFW_PRESS, 0);
    EXPECT_TRUE(copy->pressed(KeyCode::A));
    copy.reset();
    EXPECT_EQ(window.key_callback, nullptr);
    EXPECT_TRUE(PRESSED_KEYS.empty());
}
TEST_F(KeyboardTest, DestroyingCopyFirstKeepsOriginalCallbackAndState) {
    using namespace aho::input;
    const auto previous = PRESSED_KEYS;
    struct Restore {
        std::unordered_set<KeyCode> value;
        ~Restore() { PRESSED_KEYS = value; }
    } restore{previous};
    aho::InputManager manager(window.handle());
    auto original = manager.get<KeyBoard>();
    ASSERT_NE(window.key_callback, nullptr);
    window.key_callback(window.handle(), GLFW_KEY_B, 0, GLFW_PRESS, 0);
    {
        KeyBoard copy(*original);
        EXPECT_TRUE(copy.pressed(KeyCode::B));
    }
    ASSERT_NE(window.key_callback, nullptr);
    EXPECT_TRUE(original->pressed(KeyCode::B));
    original.reset();
    EXPECT_EQ(window.key_callback, nullptr);
}

}
