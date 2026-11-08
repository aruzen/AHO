#pragma once
#include <VSL/define.hpp>
#include <string_view>

namespace VSL_NAMESPACE::operation {
    void initialize_glfw();
    void terminate_glfw() noexcept;
    void* create_os_window(std::string_view name, int width, int height);
    void destroy_os_window(void* window) noexcept;
    void detach_os_window(void* window) noexcept;
} // namespace VSL_NAMESPACE::operation
