#include <AHO/io/keyboard.hpp>
#include <optional>
#include <type_traits>
#include "../support/input_backend.hpp"
#include <concepts>

extern std::unordered_set<aho::input::KeyCode> PRESSED_KEYS;
namespace {
using KeyboardTest = aho_test::InputBackendTest;
static_assert(std::derived_from<aho::input::KeyBoard, aho::input::abstract::Characters>);
static_assert(std::derived_from<aho::input::KeyBoard,
    aho::input::abstract::Input<std::unordered_set<aho::input::KeyCode>>>);
TEST_F(KeyboardTest, PublicCharacterAndInputInterfacesReportKeyboardState) {
    using namespace aho::input;
    const auto previous = PRESSED_KEYS;
    struct Restore {
        std::unordered_set<KeyCode> value;
        ~Restore() { PRESSED_KEYS = value; }
    } restore{previous};
    PRESSED_KEYS.clear();
    aho::InputManager manager(window.handle());
    {
        const auto keyboard = manager.get<KeyBoard>();
        const std::shared_ptr<abstract::Characters> characters = keyboard;
        const std::shared_ptr<abstract::Input<std::unordered_set<KeyCode>>> input = keyboard;
        ASSERT_NE(window.key_callback, nullptr);
        EXPECT_TRUE(characters->state().empty());
        window.key_callback(window.handle(), GLFW_KEY_A, 0, GLFW_PRESS, 0);
        window.key_callback(window.handle(), GLFW_KEY_B, 0, GLFW_PRESS, 0);
        const std::unordered_set<KeyCode> both{KeyCode::A, KeyCode::B};
        EXPECT_EQ(characters->state(), both);
        EXPECT_EQ(input->state(), both);
        window.key_callback(window.handle(), GLFW_KEY_A, 0, GLFW_RELEASE, 0);
        EXPECT_EQ(characters->state(), (std::unordered_set<KeyCode>{KeyCode::B}));
        EXPECT_EQ(input->state(), keyboard->state());
    }
    EXPECT_EQ(window.key_callback, nullptr);
}

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
    static_assert(std::is_nothrow_copy_constructible_v<aho::input::KeyBoard>);
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
