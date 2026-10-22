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
        int cursor_mode = GLFW_CURSOR_NORMAL, raw_motion = GLFW_FALSE, focused = GLFW_TRUE;
        inline static bool RAW_SUPPORTED = true;
        GLFWkeyfun key_callback = nullptr;
        GLFWscrollfun scroll_callback = nullptr;
        inline static std::unordered_map<GLFWwindow*, InputWindow*> WINDOWS;
        inline static unsigned INVALID_HANDLES = 0;
        InputWindow() { WINDOWS.emplace(handle(), this); }
        ~InputWindow() { WINDOWS.erase(handle()); }
        InputWindow(const InputWindow&) = delete;
        InputWindow& operator=(const InputWindow&) = delete;
        GLFWwindow* handle() { return reinterpret_cast<GLFWwindow*>(this); }
        static InputWindow* Find(GLFWwindow* handle) {
            const auto found = WINDOWS.find(handle);
            if (found != WINDOWS.end())
                return found->second;
            ++INVALID_HANDLES;
            return nullptr;
        }
    };
    class InputBackendTest : public testing::Test {
    protected:
        InputWindow window;
        unsigned invalid_handles_before = 0;
        void SetUp() override { invalid_handles_before = InputWindow::INVALID_HANDLES; }
        void TearDown() override {
            EXPECT_EQ(InputWindow::INVALID_HANDLES, invalid_handles_before)
                << "Input objects must keep the window handle through callback unregistration.";
        }
    };
} // namespace aho_test
