#pragma once
#include <VSL/define.hpp>
#include <string_view>

struct GLFWwindow;

namespace VSL_NAMESPACE::operation {
    void initialize_glfw();
    void terminate_glfw() noexcept;
    void* create_os_window(std::string_view name, int width, int height);
    void destroy_os_window(void* window) noexcept;
    void detach_os_window(void* window) noexcept;
    void poll_os_windows();
    void resize_os_window(void* window, int width, int height);
    bool os_window_should_close(void* window);
    void set_os_window_should_close(void* window, bool value);
    void set_os_window_resize_callback(void* window, void (*callback)(GLFWwindow*, int, int));
} // namespace VSL_NAMESPACE::operation
