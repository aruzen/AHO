#include <VSL/operation/window_ops.hpp>
#include <VSL/exceptions.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    namespace {
        [[noreturn]] void throw_glfw_error(const char* operation) {
            const char* description = nullptr;
            const int error = glfwGetError(&description);
            throw exceptions::RuntimeException("GLFW", std::string(operation) + ": " + (description ? description : "Unknown error") + " (error " + std::to_string(error) + ")");
        }
    } // namespace
    void initialize_glfw() {
        if (glfwInit() != GLFW_TRUE)
            throw_glfw_error("Failed to initialize GLFW");
    }
    void terminate_glfw() noexcept {
        glfwTerminate();
    }
    void* create_os_window(std::string_view name, int width, int height) {
        if (name.find('\0') != std::string_view::npos)
            throw std::invalid_argument("Window name contains NUL");
        const std::string title(name);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        auto* window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!window)
            throw_glfw_error("Failed to create GLFW window");
        return window;
    }
    void destroy_os_window(void* window) noexcept {
        if (window)
            glfwDestroyWindow(static_cast<GLFWwindow*>(window));
    }
    void detach_os_window(void* window) noexcept {
        glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(window), nullptr);
        glfwHideWindow(static_cast<GLFWwindow*>(window));
    }
} // namespace VSL_NAMESPACE::operation
