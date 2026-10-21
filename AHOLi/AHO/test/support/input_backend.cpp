#include "input_backend.hpp"

extern "C" {
int glfwRawMouseMotionSupported() { return aho_test::InputWindow::raw_supported ? GLFW_TRUE : GLFW_FALSE; }
int glfwGetWindowAttrib(GLFWwindow* handle, int attribute) {
    auto* window = aho_test::InputWindow::find(handle);
    return window && attribute == GLFW_FOCUSED ? window->focused : 0;
}
int glfwGetInputMode(GLFWwindow* handle, int mode) {
    auto* window = aho_test::InputWindow::find(handle);
    if (!window) return 0;
    return mode == GLFW_CURSOR ? window->cursor_mode : window->raw_motion;
}
void glfwSetInputMode(GLFWwindow* handle, int mode, int value) {
    auto* window = aho_test::InputWindow::find(handle);
    if (!window) return;
    if (mode == GLFW_CURSOR) window->cursor_mode = value;
    if (mode == GLFW_RAW_MOUSE_MOTION) window->raw_motion = value;
}
const char* glfwGetKeyName(int key, int) {
    switch (key) {
        case GLFW_KEY_A:
            return "test-layout-a";
        case GLFW_KEY_B:
            return "test-layout-b";
        default:
            return nullptr;
    }
}
int glfwGetKey(GLFWwindow* handle, int key) {
    auto* window = aho_test::InputWindow::find(handle);
    return window ? window->keys.at(key) : GLFW_RELEASE;
}
int glfwGetMouseButton(GLFWwindow* handle, int button) {
    auto* window = aho_test::InputWindow::find(handle);
    return window ? window->buttons.at(button) : GLFW_RELEASE;
}
void glfwGetCursorPos(GLFWwindow* handle, double* x, double* y) {
    auto* window = aho_test::InputWindow::find(handle);
    if (window) {
        *x = window->cursor_x;
        *y = window->cursor_y;
    }
}
GLFWkeyfun glfwSetKeyCallback(GLFWwindow* handle, GLFWkeyfun callback) {
    auto* window = aho_test::InputWindow::find(handle);
    if (!window)
        return nullptr;
    return std::exchange(window->key_callback, callback);
}
GLFWscrollfun glfwSetScrollCallback(GLFWwindow* handle, GLFWscrollfun callback) {
    auto* window = aho_test::InputWindow::find(handle);
    if (!window)
        return nullptr;
    return std::exchange(window->scroll_callback, callback);
}
}
