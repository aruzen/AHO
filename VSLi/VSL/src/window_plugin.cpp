#include <VSL/window_plugin.hpp>
#include <VSL/utils/string.hpp>
#include <VSL/lifetime/glfw_lease.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <utility>

bool VSL_NAMESPACE::window_plugin::QuietClose::on_update(VSL_NAMESPACE::PureWindow window) {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    auto data = window.data_;
    if (data && data->window_handle && glfwWindowShouldClose((GLFWwindow*)data->window_handle)) {
        VSL_NAMESPACE::logging("destroy " + data->name + " by QuietClosePlugin");
        window.close();
    }
    return true;
}

VSL_NAMESPACE::window_plugin::QuietClose::QuietClose(VSL_NAMESPACE::PureWindow*) {
    id = QUIET_CLOSE_USED_ID;
}

bool VSL_NAMESPACE::window_plugin::HookShouldClose::on_update(VSL_NAMESPACE::PureWindow window) {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    const auto data = window.data_;
    if (data && data->window_handle && glfwWindowShouldClose((GLFWwindow*)data->window_handle)) {
        callback(window);
    }
    return true;
}

void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
    VSL_NAMESPACE::lifetime::GLFWCallbackScope callback_scope;
    for (auto& wd : VSL_NAMESPACE::PureWindow::WINDOWS) {
        if ((GLFWwindow*)wd->window_handle != window)
            continue;
        if (!wd->plugins.contains(VSL_NAMESPACE::window_plugin::HookResize::HOOK_RESIZE_USED_ID))
            return;
        const auto data = wd;
        const auto owner = data->os_window;
        auto p = data->plugins.at(VSL_NAMESPACE::window_plugin::HookResize::HOOK_RESIZE_USED_ID);
        if (auto hook = std::dynamic_pointer_cast<VSL_NAMESPACE::window_plugin::HookResize>(p); hook)
            hook->callback(VSL_NAMESPACE::PureWindow(data), width, height);
        // callbackでWINDOWSが再確保される可能性があるため、走査を継続しない。
        return;
    }
}

VSL_NAMESPACE::window_plugin::HookResize::HookResize(VSL_NAMESPACE::PureWindow* window, std::function<void(PureWindow, int, int)> callback) : callback(std::move(callback)) {
    if (!window)
        throw std::invalid_argument("Resize hook requires a live window");
    const auto owner = window->window_owner();
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    id = HOOK_RESIZE_USED_ID;
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(owner.native_handle()), framebuffer_resize_callback);
}
