#include <AHO/io/mouse.hpp>
#include <optional>
#include <type_traits>
#include "../support/input_backend.hpp"
#include "../support/geometry.hpp"

namespace {
using MouseTest = aho_test::InputBackendTest;
struct RestoreWheelState {
    std::size_t generation = aho::input::MouseWheel::_generation;
    aho::d2::VectorD state = aho::input::MouseWheel::_state;
    ~RestoreWheelState() {
        aho::input::MouseWheel::_generation = generation;
        aho::input::MouseWheel::_state = state;
    }
};

TEST_F(MouseTest, ButtonConstructorsAndTransitionsReflectBackendState) {
    using namespace aho::input;
    aho::InputManager manager(window.handle());
    const auto button = manager.get<MouseButton>(MouseCode::LeftClick);
    const auto typed = manager.get<MouseButton>(MouseID(MouseCode::RightClick));
    EXPECT_EQ(button->state(), ButtonState::Idle); EXPECT_EQ(typed->state(), ButtonState::Idle);
    window.buttons[GLFW_MOUSE_BUTTON_LEFT] = GLFW_PRESS;
    button->update();
    EXPECT_TRUE(button->down()); EXPECT_TRUE(button->pressed()); EXPECT_FALSE(button->up());
    button->update();
    EXPECT_EQ(button->state(), ButtonState::Pressed); EXPECT_FALSE(button->down());
    window.buttons[GLFW_MOUSE_BUTTON_LEFT] = GLFW_RELEASE;
    button->update();
    EXPECT_TRUE(button->up()); EXPECT_FALSE(button->pressed());
    button->update(); EXPECT_EQ(button->state(), ButtonState::Idle);
}
TEST_F(MouseTest, CursorSamplesPositionAndReportsNewMinusPreviousDelta) {
    aho::InputManager manager(window.handle());
    window.cursor_x = 10; window.cursor_y = 20;
    const auto cursor = manager.get<aho::input::Cursor>();
    aho_test::expect_coordinates(cursor->state(), 10, 20);
    window.cursor_x = 13; window.cursor_y = 16;
    cursor->update();
    aho_test::expect_coordinates(cursor->state(), 13, 16);
    aho_test::expect_coordinates(cursor->delta(), 3, -4);
    cursor->update(); aho_test::expect_coordinates(cursor->delta(), 0, 0);
}
TEST_F(MouseTest, CursorDeltaPreservesFractionalMovementInBothDirections) {
    aho::InputManager manager(window.handle());
    window.cursor_x = -2.5;
    window.cursor_y = 4.75;
    const auto cursor = manager.get<aho::input::Cursor>();
    aho_test::expect_coordinates(cursor->delta(), 0, 0);
    window.cursor_x = -6.25;
    window.cursor_y = 10.5;
    cursor->update();
    aho_test::expect_coordinates(cursor->state(), -6.25, 10.5);
    aho_test::expect_coordinates(cursor->delta(), -3.75, 5.75);
    window.cursor_x = -2.5;
    window.cursor_y = 4.75;
    cursor->update();
    aho_test::expect_coordinates(cursor->delta(), 3.75, -5.75);
    cursor->update();
    aho_test::expect_coordinates(cursor->delta(), 0, 0);
}
TEST_F(MouseTest, WheelPublishesScrollThenResetsAfterAnUpdate) {
    using aho::input::MouseWheel;
    RestoreWheelState restore;
    aho::InputManager manager(window.handle());
    {
        const auto wheel = manager.get<MouseWheel>();
        ASSERT_NE(window.scroll_callback, nullptr);
        aho_test::expect_coordinates(wheel->state(), 0, 0);
        window.scroll_callback(window.handle(), 1, -2);
        wheel->update();
        aho_test::expect_coordinates(wheel->state(), 1, -2);
        wheel->update();
        aho_test::expect_coordinates(wheel->state(), 0, 0);
    }
    EXPECT_EQ(window.scroll_callback, nullptr);
}
TEST_F(MouseTest, CopiedWheelRetainsSubscriptionUntilBothObjectsAreDestroyed) {
    using aho::input::MouseWheel;
    static_assert(std::is_nothrow_copy_constructible_v<MouseWheel>);
    RestoreWheelState restore;
    aho::InputManager manager(window.handle());
    auto original = manager.get<MouseWheel>();
    std::optional<MouseWheel> copy(std::in_place, *original);
    original.reset();
    ASSERT_NE(window.scroll_callback, nullptr);
    window.scroll_callback(window.handle(), 1.5, -2.5);
    aho_test::expect_coordinates(copy->state(), 1.5, -2.5);
    copy.reset();
    EXPECT_EQ(window.scroll_callback, nullptr);
    const auto next = manager.get<MouseWheel>();
    EXPECT_NE(window.scroll_callback, nullptr);
}
TEST_F(MouseTest, DestroyingWheelCopyFirstKeepsOriginalSubscription) {
    using aho::input::MouseWheel;
    RestoreWheelState restore;
    aho::InputManager manager(window.handle());
    auto original = manager.get<MouseWheel>();
    {
        MouseWheel copy(*original);
    }
    ASSERT_NE(window.scroll_callback, nullptr);
    window.scroll_callback(window.handle(), -1, 2);
    aho_test::expect_coordinates(original->state(), -1, 2);
    original.reset();
    EXPECT_EQ(window.scroll_callback, nullptr);
}
TEST_F(MouseTest, WheelCopyPreservesPendingScrollAndConsumesItIndependently) {
    using aho::input::MouseWheel;
    RestoreWheelState restore;
    aho::InputManager manager(window.handle());
    const auto original = manager.get<MouseWheel>();
    ASSERT_NE(window.scroll_callback, nullptr);
    window.scroll_callback(window.handle(), 3.5, -4.5);
    MouseWheel copy(*original);
    aho_test::expect_coordinates(copy.state(), 3.5, -4.5);
    copy.update();
    aho_test::expect_coordinates(copy.state(), 0, 0);
    aho_test::expect_coordinates(original->state(), 3.5, -4.5);
    original->update();
    aho_test::expect_coordinates(original->state(), 0, 0);
}
TEST_F(MouseTest, AggregateMouseReturnsItsManagedInputs) {
    aho::InputManager manager(window.handle());
    const auto mouse = manager.get<aho::input::Mouse>();
    EXPECT_NE(mouse->cursor(), nullptr); EXPECT_NE(mouse->wheel(), nullptr);
    EXPECT_NE(mouse->leftClick(), nullptr); EXPECT_NE(mouse->rightClick(), nullptr); EXPECT_NE(mouse->middleButton(), nullptr);
    EXPECT_NE(mouse->leftClick(), mouse->rightClick());
    EXPECT_EQ(manager.update_targets.size(), 5u);
}
}
