#include <VSL/define.hpp>
#include <VSL/window.hpp>
#include <algorithm>

#include <VSL/exceptions.hpp>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifndef _MSC_VER
#include <unistd.h>
#else
#endif

#include <VSL/window_plugin.hpp>
#include <VSL/vulkan/surface.hpp>

std::vector<std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData>> VSL_NAMESPACE::PureWindow::WINDOWS;
bool VSL_NAMESPACE::PureWindow::INITED = false;

namespace {
    [[noreturn]] void throw_glfw_error(const char* operation) {
        const char* description = nullptr;
        const int error = glfwGetError(&description);
        throw VSL_NAMESPACE::exceptions::RuntimeException("GLFW",
                                                          std::string(operation) + ": " + (description ? description : "Unknown error") +
                                                              " (error " + std::to_string(error) + ")");
    }
} // namespace

VSL_NAMESPACE::PureWindow::PureWindow(std::string name, int width, int height) {
    data_ = std::make_shared<WindowData>();
    data_->height = height;
    data_->width = width;
    data_->name = std::move(name);

    if (!INITED) {
        if (glfwInit() != GLFW_TRUE)
            throw_glfw_error("Failed to initialize GLFW");
        INITED = true;
    }
    try {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        data_->window_handle = glfwCreateWindow(width, height, data_->name.c_str(), nullptr, nullptr);
        if (!data_->window_handle)
            throw_glfw_error("Failed to create GLFW window");
        WINDOWS.push_back(data_);
    } catch (...) {
        if (data_->window_handle) {
            glfwDestroyWindow(static_cast<GLFWwindow*>(data_->window_handle));
            data_->window_handle = nullptr;
        }
        if (WINDOWS.empty()) {
            glfwTerminate();
            INITED = false;
        }
        throw;
    }
}

VSL_NAMESPACE::PureWindow::PureWindow(std::shared_ptr<WindowData> data_) : data_(data_) {}

bool VSL_NAMESPACE::PureWindow::Update() {
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
    glfwPollEvents();
    return true;
}

bool VSL_NAMESPACE::PureWindow::close() {
    if (!data_ || !data_->window_handle)
        return false;

    data_->destroy();

    return true;
}

std::string VSL_NAMESPACE::PureWindow::name() {
    return data_->name;
}

VSL_NAMESPACE::PureWindow::operator bool() {
    return data_ && data_->window_handle != nullptr;
}

bool VSL_NAMESPACE::PureWindow::operator==(const PureWindow& o) {
    return data_ == o.data_;
}

bool VSL_NAMESPACE::PureWindow::operator==(const PureWindow::WindowData* o) {
    return data_.get() == o;
}

bool VSL_NAMESPACE::PureWindow::resize(int width, int height) {
    glfwSetWindowSize((GLFWwindow*)data_->window_handle, width, height);
    return true;
}

void VSL_NAMESPACE::PureWindow::WindowData::destroy() {
    if (!window_handle)
        return;
    for (auto i = WINDOWS.begin(); i != WINDOWS.end(); i++)
        if (i->get() == this) {
            WINDOWS.erase(i);
            break;
        }
    glfwDestroyWindow((GLFWwindow*)window_handle);

    window_handle = nullptr;

    if (WINDOWS.size() == 0) {
        glfwTerminate();
        INITED = false;
    }
}

VSL_NAMESPACE::Window::Window(std::string name, int width, int height) : PureWindow(name, width, height) {
    add_plugin<VSL_NAMESPACE::window_plugin::QuietClose>();
}

VSL_NAMESPACE::Window::Window(std::shared_ptr<WindowData> data) : PureWindow(data) {
    if (not data->plugins.contains(VSL_NAMESPACE::window_plugin::QuietClose::QUIET_CLOSE_USED_ID))
        add_plugin<VSL_NAMESPACE::window_plugin::QuietClose>();
}
