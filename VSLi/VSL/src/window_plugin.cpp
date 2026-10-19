#include <VSL/window_plugin.hpp>
#include <VSL/utils/string.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <utility>

bool VSL_NAMESPACE::window_plugin::QuietClose::onUpdate(VSL_NAMESPACE::PureWindow window) {
    auto data = window._data;
    if (glfwWindowShouldClose((GLFWwindow*)data->window_handle)) {
        VSL_NAMESPACE::logging("destroy " + data->name + " by QuietClosePlugin");
        data->destroy();
    }
    return true;
}

VSL_NAMESPACE::window_plugin::QuietClose::QuietClose(VSL_NAMESPACE::PureWindow*) {
    id = QUIET_CLOSE_USED_ID;
}

bool VSL_NAMESPACE::window_plugin::HookShouldClose::onUpdate(VSL_NAMESPACE::PureWindow window) {
    auto&& data = window._data;
    if (glfwWindowShouldClose((GLFWwindow*)data->window_handle)) {
        callback(window);
    }
    return true;
}

void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
    for (auto& wd : VSL_NAMESPACE::PureWindow::WINDOWS) {
        if ((GLFWwindow*)wd->window_handle != window)
            continue;
        if (!wd->plugins.contains(VSL_NAMESPACE::window_plugin::HookResize::HOOK_RESIZE_USED_ID))
            return;
        auto p = wd->plugins[VSL_NAMESPACE::window_plugin::HookResize::HOOK_RESIZE_USED_ID];
        if (auto hook = std::dynamic_pointer_cast<VSL_NAMESPACE::window_plugin::HookResize>(p); hook)
            hook->callback(VSL_NAMESPACE::PureWindow(wd), width, height);
    }
}

VSL_NAMESPACE::window_plugin::HookResize::HookResize(VSL_NAMESPACE::PureWindow* window, std::function<void(PureWindow, int, int)> callback) : callback(std::move(callback)) {
    id = HOOK_RESIZE_USED_ID;
    glfwSetFramebufferSizeCallback((GLFWwindow*)window->_data->window_handle, framebuffer_resize_callback);
}
