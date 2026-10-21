#include <AHO/window.hpp>
#include <GLFW/glfw3.h>
#include <VSL/exceptions.hpp>
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif

#include <utility>
#include <vector>

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::GetMonitorSize() {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Primary monitor is not available");
#ifdef __APPLE__
#if defined(GLFW_PLATFORM_COCOA)
    if (glfwGetPlatform() == GLFW_PLATFORM_COCOA) {
#endif
        auto mode = CGDisplayCopyDisplayMode(glfwGetCocoaMonitor(monitor));
        if (!mode)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Primary monitor video mode is not available");
        const auto width = static_cast<int>(CGDisplayModeGetWidth(mode));
        const auto height = static_cast<int>(CGDisplayModeGetHeight(mode));
        CGDisplayModeRelease(mode);
        return {width, height};
#if defined(GLFW_PLATFORM_COCOA)
    }
#endif
#endif
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Primary monitor video mode is not available");
    return {mode->width, mode->height};
}

AHO_NAMESPACE::window::Window::Window(std::string name, int width, int height, AHO_NAMESPACE::engine::EngineAccessor _engine)
    : VSL_NAMESPACE::Window(name, width, height) {
    engine = _engine;
    auto surface = this->add_plugin<VSL_NAMESPACE::Surface>(_engine._data->vulkan_instance);
    VSL_NAMESPACE::Swapchain swapchain(_engine._data->logical_device, surface);
    VSL_NAMESPACE::RenderPass pass(swapchain);

    _data2 = std::make_shared<AHO_NAMESPACE::window::WindowData>(AHO_NAMESPACE::window::WindowData{
        .surface = surface,
        .swapchain = swapchain,
        .render_pass = pass,
        .frame_buffer = VSL_NAMESPACE::FrameBuffer(swapchain, pass),
        .image_available = _engine._data->synchro_manager
                               .createSemaphore(name + "ImageAvailable", _engine._data->command_manager.getBuffer().getSize()),
        .render_finished = _engine._data->synchro_manager
                               .createSemaphore(name + "RenderFinished", swapchain.getSwapImageSize()),
        .in_flight = _engine._data->synchro_manager
                         .createFence(name + "InFlight", _engine._data->command_manager.getBuffer().getSize(), true)});
}

AHO_NAMESPACE::window::Window::Window(engine::EngineAccessor engine, std::shared_ptr<VSL_NAMESPACE::Window::WindowData> vsl_data,
                                      std::shared_ptr<AHO_NAMESPACE::window::WindowData> aho_data)
    : VSL_NAMESPACE::Window(std::move(vsl_data)), engine(std::move(engine)) {
    _data2 = std::move(aho_data);
}

bool AHO_NAMESPACE::window::Window::resize(int width, int height) {
    if (not VSL_NAMESPACE::PureWindow::resize(width, height))
        return false;
    engine._data->command_manager.waitIdle();
    const auto usage = _data2->swapchain.images.at(0).getUsage();
    _data2->swapchain = VSL_NAMESPACE::Swapchain(engine._data->logical_device, _data2->surface, _data2->swapchain,
                                                 std::nullopt, std::nullopt, usage);
    _data2->frame_buffer = VSL_NAMESPACE::FrameBuffer(_data2->swapchain, _data2->render_pass);
    if (_data2->render_finished.size() != _data2->swapchain.getSwapImageSize())
        _data2->render_finished = engine._data->synchro_manager.createSemaphore(
            name() + "RenderFinished", _data2->swapchain.getSwapImageSize());
    const auto data = _data;
    std::vector<std::pair<unsigned char, std::weak_ptr<WindowResizeHookPlugin>>> hooks;
    hooks.reserve(data->plugins.size());
    for (const auto& [id, plugin] : data->plugins)
        if (auto hook = std::dynamic_pointer_cast<WindowResizeHookPlugin>(plugin))
            hooks.emplace_back(id, hook);
    for (const auto& [id, candidate] : hooks) {
        if (_data != data || !_data2 || !data->window_handle)
            break;
        auto hook = candidate.lock();
        const auto current = data->plugins.find(id);
        if (!hook || current == data->plugins.end() || current->second != hook)
            continue;
        hook->hook(this);
    }
    return true;
}

void AHO_NAMESPACE::window::Window::maximize() {
    glfwMaximizeWindow((GLFWwindow*)_data->window_handle);
}

void AHO_NAMESPACE::window::Window::restore() {
    glfwRestoreWindow((GLFWwindow*)_data->window_handle);
}

void AHO_NAMESPACE::window::Window::show() {
    if (!*this)
        throw std::logic_error("Cannot show a closed window");
    glfwShowWindow(static_cast<GLFWwindow*>(_data->window_handle));
}

void AHO_NAMESPACE::window::Window::focus() {
    if (!*this)
        throw std::logic_error("Cannot focus a closed window");
    glfwFocusWindow(static_cast<GLFWwindow*>(_data->window_handle));
}

bool AHO_NAMESPACE::window::Window::set_title(const std::string& title) {
    glfwSetWindowTitle((GLFWwindow*)_data->window_handle, title.c_str());
    return true;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::window_size() {
    d2::VectorI v;
    glfwGetWindowSize((GLFWwindow*)_data->window_handle, &v.value.x.value, &v.value.y.value);
    return v;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::frame_size() {
    d2::VectorI v;
    glfwGetFramebufferSize((GLFWwindow*)_data->window_handle, &v.value.x.value, &v.value.y.value);
    return v;
}

AHO_NAMESPACE::window::WindowResizeHookPlugin::WindowResizeHookPlugin(VSL_NAMESPACE::PureWindow* w,
                                                                      std::function<void(AHO_NAMESPACE::window::Window*)> hook)
    : hook(std::move(hook)) {
}

bool AHO_NAMESPACE::window::Window::close() {
    if (_data2)
        engine._data->command_manager.waitIdle();
    _data2.reset();
    return VSL_NAMESPACE::PureWindow::close();
}

AHO_NAMESPACE::window::WindowData::~WindowData() {
}
