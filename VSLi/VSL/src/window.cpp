#include <VSL/define.hpp>
#include <VSL/window.hpp>
#include <algorithm>

#include <VSL/exceptions.hpp>
#include <VSL/lifetime/window_runtime.hpp>
#include <VSL/operation/window_ops.hpp>
#include <utility>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifndef _MSC_VER
#include <unistd.h>
#else
#endif

#include <VSL/window_plugin.hpp>
#include <VSL/vulkan/surface.hpp>

std::vector<std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData>> VSL_NAMESPACE::PureWindow::WINDOWS;

VSL_NAMESPACE::PureWindow::PureWindow(std::string name, int width, int height) {
    lifetime::GLFWCallScope scope;
    data_ = std::make_shared<WindowData>();
    data_->height = height;
    data_->width = width;
    data_->name = std::move(name);

    auto owner = lifetime::OSWindowOwner::Create(data_->name, width, height);
    WINDOWS.push_back(data_);
    data_->window_handle = owner.native_handle();
    data_->os_window = std::move(owner);
}

VSL_NAMESPACE::PureWindow::PureWindow(std::shared_ptr<WindowData> data_) : data_(data_) {}

bool VSL_NAMESPACE::PureWindow::Update() {
    lifetime::GLFWCallScope scope;
    if (lifetime::in_glfw_callback())
        throw std::logic_error("Cannot poll GLFW from a callback");
    if (lifetime::glfw_runtime_active())
        glfwPollEvents();
    bool s = true;
    const std::vector<std::weak_ptr<WindowData>> windows(WINDOWS.begin(), WINDOWS.end());
    for (const auto& candidate : windows) {
        auto p = candidate.lock();
        if (!p || std::ranges::find(WINDOWS, p) == WINDOWS.end())
            continue;
        const std::vector<std::weak_ptr<OnUpdate>> callbacks(p->update.begin(), p->update.end());
        for (const auto& callback : callbacks) {
            if (std::ranges::find(WINDOWS, p) == WINDOWS.end())
                break;
            auto listener = callback.lock();
            if (!listener || std::ranges::find(p->update, listener) == p->update.end())
                continue;
            if (!listener->on_update(PureWindow(p)))
                s = false;
        }
    }
    return s;
}

bool VSL_NAMESPACE::PureWindow::NoCheckUpdate() {
    lifetime::GLFWCallScope scope;
    if (lifetime::in_glfw_callback())
        throw std::logic_error("Cannot poll GLFW from a callback");
    if (lifetime::glfw_runtime_active())
        glfwPollEvents();
    return true;
}

bool VSL_NAMESPACE::PureWindow::close() {
    const auto data = data_;
    if (!data || !data->window_handle)
        return false;
    data->destroy();

    return true;
}

std::string VSL_NAMESPACE::PureWindow::name() {
    return data_->name;
}

VSL_NAMESPACE::PureWindow::operator bool() const {
    return data_ && data_->window_handle != nullptr;
}

bool VSL_NAMESPACE::PureWindow::operator==(const PureWindow& o) {
    return data_ == o.data_;
}

bool VSL_NAMESPACE::PureWindow::operator==(const PureWindow::WindowData* o) {
    return data_.get() == o;
}

bool VSL_NAMESPACE::PureWindow::resize(int width, int height) {
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Window size must be positive");
    lifetime::GLFWCallScope scope;
    const auto data = data_;
    if (!data || !data->window_handle)
        return false;
    const auto owner = data->os_window;
    glfwSetWindowSize(static_cast<GLFWwindow*>(owner.native_handle()), width, height);
    return data->window_handle != nullptr;
}

VSL_NAMESPACE::lifetime::OSWindowOwner VSL_NAMESPACE::PureWindow::window_owner() const {
    if (!data_ || !data_->window_handle || !data_->os_window)
        throw exceptions::RuntimeException("Window", "A live managed window is required");
    return data_->os_window;
}

void VSL_NAMESPACE::PureWindow::WindowData::destroy() {
    if (!window_handle)
        return;
    const auto registered = std::ranges::find_if(WINDOWS, [this](const auto& data) { return data.get() == this; });
    const auto keep = registered == WINDOWS.end() ? std::shared_ptr<WindowData>{} : *registered;
    lifetime::GLFWCallScope scope;
    auto owner = std::move(os_window);
    const auto native = window_handle;
    window_handle = nullptr;
    std::erase_if(WINDOWS, [this](const auto& data) { return data.get() == this; });
    auto detached_plugins = std::move(plugins);
    auto detached_updates = std::move(update);
    operation::detach_os_window(native);
}

VSL_NAMESPACE::Window::Window(std::string name, int width, int height) : PureWindow(name, width, height) {
    add_plugin<VSL_NAMESPACE::window_plugin::QuietClose>();
}

VSL_NAMESPACE::Window::Window(std::shared_ptr<WindowData> data) : PureWindow(data) {
    if (not data->plugins.contains(VSL_NAMESPACE::window_plugin::QuietClose::QUIET_CLOSE_USED_ID))
        add_plugin<VSL_NAMESPACE::window_plugin::QuietClose>();
}
