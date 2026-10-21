#include "input_backend.hpp"

extern "C" {
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
    auto* window = aho_test::InputWindow::Find(handle);
    return window ? window->keys.at(key) : GLFW_RELEASE;
}
int glfwGetMouseButton(GLFWwindow* handle, int button) {
    auto* window = aho_test::InputWindow::Find(handle);
    return window ? window->buttons.at(button) : GLFW_RELEASE;
}
void glfwGetCursorPos(GLFWwindow* handle, double* x, double* y) {
    auto* window = aho_test::InputWindow::Find(handle);
    if (window) {
        *x = window->cursor_x;
        *y = window->cursor_y;
    }
}
GLFWkeyfun glfwSetKeyCallback(GLFWwindow* handle, GLFWkeyfun callback) {
    auto* window = aho_test::InputWindow::Find(handle);
    if (!window)
        return nullptr;
    return std::exchange(window->key_callback, callback);
}
GLFWscrollfun glfwSetScrollCallback(GLFWwindow* handle, GLFWscrollfun callback) {
    auto* window = aho_test::InputWindow::Find(handle);
    if (!window)
        return nullptr;
    return std::exchange(window->scroll_callback, callback);
}
}
