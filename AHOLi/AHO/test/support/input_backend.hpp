#pragma once
#include <gtest/gtest.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <array>
#include <unordered_map>

namespace aho_test {
    struct InputWindow {
        std::array<int, GLFW_KEY_LAST + 1> keys{};
        std::array<int, GLFW_MOUSE_BUTTON_LAST + 1> buttons{};
        double cursor_x = 0, cursor_y = 0;
        GLFWkeyfun key_callback = nullptr;
        GLFWscrollfun scroll_callback = nullptr;
        inline static std::unordered_map<GLFWwindow*, InputWindow*> windows;
        inline static unsigned invalid_handles = 0;
        InputWindow() { windows.emplace(handle(), this); }
        ~InputWindow() { windows.erase(handle()); }
        InputWindow(const InputWindow&) = delete;
        InputWindow& operator=(const InputWindow&) = delete;
        GLFWwindow* handle() { return reinterpret_cast<GLFWwindow*>(this); }
        static InputWindow* find(GLFWwindow* handle) {
            const auto found = windows.find(handle);
            if (found != windows.end())
                return found->second;
            ++invalid_handles;
            return nullptr;
        }
    };
    class InputBackendTest : public testing::Test {
    protected:
        InputWindow window;
        unsigned invalid_handles_before = 0;
        void SetUp() override { invalid_handles_before = InputWindow::invalid_handles; }
        void TearDown() override {
            EXPECT_EQ(InputWindow::invalid_handles, invalid_handles_before)
                << "Input objects must keep the window handle through callback unregistration.";
        }
    };
} // namespace aho_test
