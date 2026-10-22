#include <AHO/io/mouse.hpp>
#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
namespace {
    using CursorWindowIntegration = aho_test::LiveGPUTest;
    TEST_F(CursorWindowIntegration, CaptureModesAreSharedAndRawInputIsOptional) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        aho::InputManager manager(window);
        const auto first = manager.get<aho::input::Cursor>();
        const auto second = manager.get<aho::input::Cursor>();
        auto* native = static_cast<GLFWwindow*>(window.data_->window_handle);
        EXPECT_FALSE(first->captured());
        EXPECT_EQ(first->focused(), glfwGetWindowAttrib(native, GLFW_FOCUSED) == GLFW_TRUE);
        first->set_captured(true);
        EXPECT_TRUE(first->captured());
        EXPECT_TRUE(second->captured());
        EXPECT_EQ(glfwGetInputMode(native, GLFW_RAW_MOUSE_MOTION),
                  glfwRawMouseMotionSupported() ? GLFW_TRUE : GLFW_FALSE);
        manager.update();
        EXPECT_DOUBLE_EQ(first->delta().value.___AN1, 0);
        EXPECT_DOUBLE_EQ(second->delta().value.___AN1, 0);
        second->set_captured(false);
        EXPECT_FALSE(first->captured());
        EXPECT_FALSE(second->captured());
        EXPECT_EQ(glfwGetInputMode(native, GLFW_CURSOR), GLFW_CURSOR_NORMAL);
        EXPECT_EQ(glfwGetInputMode(native, GLFW_RAW_MOUSE_MOTION), GLFW_FALSE);
    }
} // namespace
#else
#include <optional>
#include <type_traits>
#include "../support/input_backend.hpp"
#include "../support/geometry.hpp"

namespace {
    using MouseTest = aho_test::InputBackendTest;
    struct RestoreWheelState {
        std::size_t generation = aho::input::MouseWheel::GENERATION;
        aho::d2::VectorD state = aho::input::MouseWheel::STATE;
        ~RestoreWheelState() {
            aho::input::MouseWheel::GENERATION = generation;
            aho::input::MouseWheel::STATE = state;
        }
    };

    TEST_F(MouseTest, ButtonConstructorsAndTransitionsReflectBackendState) {
        using namespace aho::input;
        aho::InputManager manager(window.handle());
        const auto button = manager.get<MouseButton>(MouseCode::LEFT_CLICK);
        const auto typed = manager.get<MouseButton>(MouseID(MouseCode::RIGHT_CLICK));
        EXPECT_EQ(button->state(), ButtonState::IDLE);
        EXPECT_EQ(typed->state(), ButtonState::IDLE);
        window.buttons[GLFW_MOUSE_BUTTON_LEFT] = GLFW_PRESS;
        button->update();
        EXPECT_TRUE(button->down());
        EXPECT_TRUE(button->pressed());
        EXPECT_FALSE(button->up());
        button->update();
        EXPECT_EQ(button->state(), ButtonState::PRESSED);
        EXPECT_FALSE(button->down());
        window.buttons[GLFW_MOUSE_BUTTON_LEFT] = GLFW_RELEASE;
        button->update();
        EXPECT_TRUE(button->up());
        EXPECT_FALSE(button->pressed());
        button->update();
        EXPECT_EQ(button->state(), ButtonState::IDLE);
    }
    TEST_F(MouseTest, CursorSamplesPositionAndReportsNewMinusPreviousDelta) {
        aho::InputManager manager(window.handle());
        window.cursor_x = 10;
        window.cursor_y = 20;
        const auto cursor = manager.get<aho::input::Cursor>();
        aho_test::expect_coordinates(cursor->state(), 10, 20);
        window.cursor_x = 13;
        window.cursor_y = 16;
        cursor->update();
        aho_test::expect_coordinates(cursor->state(), 13, 16);
        aho_test::expect_coordinates(cursor->delta(), 3, -4);
        cursor->update();
        aho_test::expect_coordinates(cursor->delta(), 0, 0);
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
    TEST_F(MouseTest, CaptureResetsWarpDeltaAndReleaseRestoresPointerMode) {
        aho::InputManager manager(window.handle());
        const auto cursor = manager.get<aho::input::Cursor>();
        EXPECT_FALSE(cursor->captured());
        cursor->set_captured(true);
        EXPECT_TRUE(cursor->captured());
        EXPECT_EQ(window.raw_motion, GLFW_TRUE);
        window.cursor_x = 400;
        window.cursor_y = 300;
        manager.update();
        aho_test::expect_coordinates(cursor->delta(), 0, 0);
        window.cursor_x += 3;
        manager.update();
        aho_test::expect_coordinates(cursor->delta(), 3, 0);
        cursor->set_captured(false);
        EXPECT_FALSE(cursor->captured());
        EXPECT_EQ(window.raw_motion, GLFW_FALSE);
        aho_test::expect_coordinates(cursor->delta(), 0, 0);
    }
    TEST_F(MouseTest, FocusLossAndRegainSuppressCameraJump) {
        aho::InputManager manager(window.handle());
        const auto cursor = manager.get<aho::input::Cursor>();
        window.focused = GLFW_FALSE;
        window.cursor_x = 100;
        manager.update();
        EXPECT_FALSE(cursor->focused());
        aho_test::expect_coordinates(cursor->delta(), 0, 0);
        window.focused = GLFW_TRUE;
        window.cursor_x = 900;
        manager.update();
        EXPECT_TRUE(cursor->focused());
        aho_test::expect_coordinates(cursor->delta(), 0, 0);
        window.cursor_x += .5;
        manager.update();
        aho_test::expect_coordinates(cursor->delta(), .5, 0);
    }
    TEST_F(MouseTest, CaptureWorksWithoutRawMouseSupport) {
        aho::InputManager manager(window.handle());
        const auto cursor = manager.get<aho::input::Cursor>();
        const bool supported = std::exchange(aho_test::InputWindow::RAW_SUPPORTED, false);
        cursor->set_captured(true);
        EXPECT_TRUE(cursor->captured());
        EXPECT_EQ(window.raw_motion, GLFW_FALSE);
        cursor->set_captured(false);
        aho_test::InputWindow::RAW_SUPPORTED = supported;
    }
    TEST_F(MouseTest, RepeatingCaptureStatePreservesCurrentAndNextMovement) {
        aho::InputManager manager(window.handle());
        const auto cursor = manager.get<aho::input::Cursor>();
        cursor->set_captured(true);
        manager.update();
        window.cursor_x = 3;
        manager.update();
        cursor->set_captured(true);
        aho_test::expect_coordinates(cursor->delta(), 3, 0);
        window.cursor_x = 5;
        manager.update();
        aho_test::expect_coordinates(cursor->delta(), 2, 0);
        cursor->set_captured(false);
        manager.update();
        window.cursor_x = 8;
        manager.update();
        cursor->set_captured(false);
        aho_test::expect_coordinates(cursor->delta(), 3, 0);
        window.cursor_x = 10;
        manager.update();
        aho_test::expect_coordinates(cursor->delta(), 2, 0);
    }
    TEST_F(MouseTest, CaptureChangesResetEveryCursorAcrossInputManagers) {
        aho::InputManager manager(window.handle()), other(window.handle());
        const auto mouse = manager.get<aho::input::Mouse>();
        const auto first = mouse->cursor();
        const auto second = other.get<aho::input::Cursor>();
        first->set_captured(true);
        EXPECT_TRUE(second->captured());
        window.cursor_x = 400;
        window.cursor_y = 300;
        manager.update();
        other.update();
        aho_test::expect_coordinates(first->delta(), 0, 0);
        aho_test::expect_coordinates(second->delta(), 0, 0);
        window.cursor_x += 2;
        manager.update();
        other.update();
        aho_test::expect_coordinates(first->delta(), 2, 0);
        aho_test::expect_coordinates(second->delta(), 2, 0);
        second->set_captured(false);
        EXPECT_FALSE(first->captured());
        aho_test::expect_coordinates(first->delta(), 0, 0);
        window.cursor_x = -40;
        other.update();
        manager.update();
        aho_test::expect_coordinates(first->delta(), 0, 0);
        aho_test::expect_coordinates(second->delta(), 0, 0);
    }
    TEST_F(MouseTest, CaptureRoundTripBeforeAnUpdateResetsEveryCursor) {
        aho::InputManager manager(window.handle());
        const auto first = manager.get<aho::input::Cursor>();
        const auto second = manager.get<aho::input::Cursor>();
        first->set_captured(true);
        first->set_captured(false);
        window.cursor_x = 1000;
        manager.update();
        aho_test::expect_coordinates(first->delta(), 0, 0);
        aho_test::expect_coordinates(second->delta(), 0, 0);
        window.cursor_x += 4;
        window.cursor_y += 5;
        manager.update();
        aho_test::expect_coordinates(first->delta(), 4, 5);
        aho_test::expect_coordinates(second->delta(), 4, 5);
    }
    TEST_F(MouseTest, CaptureChangesDoNotDiscardMovementInAnotherWindow) {
        aho_test::InputWindow other_window;
        aho::InputManager manager(window.handle()), other(other_window.handle());
        const auto first = manager.get<aho::input::Cursor>();
        const auto second = other.get<aho::input::Cursor>();
        first->set_captured(true);
        other_window.cursor_x = 3;
        other_window.cursor_y = 4;
        other.update();
        EXPECT_FALSE(second->captured());
        aho_test::expect_coordinates(second->delta(), 3, 4);
    }
    TEST_F(MouseTest, CursorCreatedAfterCaptureKeepsItsFirstRealMovement) {
        aho::InputManager manager(window.handle());
        const auto first = manager.get<aho::input::Cursor>();
        first->set_captured(true);
        window.cursor_x = 400;
        const auto second = manager.get<aho::input::Cursor>();
        window.cursor_x += 3;
        manager.update();
        aho_test::expect_coordinates(first->delta(), 0, 0);
        aho_test::expect_coordinates(second->delta(), 3, 0);
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
        EXPECT_NE(mouse->cursor(), nullptr);
        EXPECT_NE(mouse->wheel(), nullptr);
        EXPECT_NE(mouse->left_click(), nullptr);
        EXPECT_NE(mouse->right_click(), nullptr);
        EXPECT_NE(mouse->middle_button(), nullptr);
        EXPECT_NE(mouse->left_click(), mouse->right_click());
        EXPECT_EQ(manager.update_targets.size(), 5u);
    }
} // namespace
#endif
