#include <AHO/io/mouse.hpp>
#include "../support/input_backend.hpp"
#include "../support/geometry.hpp"

namespace {
using MouseTest = aho_test::InputBackendTest;
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
TEST_F(MouseTest, WheelPublishesScrollThenResetsAfterAnUpdate) {
    using aho::input::MouseWheel;
    const auto previous_generation = MouseWheel::_generation;
    const auto previous_state = MouseWheel::_state;
    struct Restore {
        std::size_t generation; aho::d2::VectorD state;
        ~Restore() { MouseWheel::_generation = generation; MouseWheel::_state = state; }
    } restore{previous_generation, previous_state};
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
TEST_F(MouseTest, AggregateMouseReturnsItsManagedInputs) {
    aho::InputManager manager(window.handle());
    const auto mouse = manager.get<aho::input::Mouse>();
    EXPECT_NE(mouse->cursor(), nullptr); EXPECT_NE(mouse->wheel(), nullptr);
    EXPECT_NE(mouse->leftClick(), nullptr); EXPECT_NE(mouse->rightClick(), nullptr); EXPECT_NE(mouse->middleButton(), nullptr);
    EXPECT_NE(mouse->leftClick(), mouse->rightClick());
    EXPECT_EQ(manager.update_targets.size(), 5u);
}
}
