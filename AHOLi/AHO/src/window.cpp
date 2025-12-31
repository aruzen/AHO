//
// Created by morimoto_hibiki on 2025/10/10.
//

#include <AHO/window.hpp>
#include <GLFW/glfw3.h>

#include <utility>

aho::d2::VectorI aho::window::GetMonitorSize() {
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    return {mode->width, mode->height};
}

aho::window::Window::Window(std::string name, int width, int height, aho::engine::EngineAccessor _engine)
        : VSL_NAMESPACE::Window(std::move(name), width, height) {
    engine = _engine;
    auto surface = this->add_plugin<VSL_NAMESPACE::Surface>(_engine._data->vulkan_instance);
    VSL_NAMESPACE::Swapchain swapchain(_engine._data->logical_device, surface);
    VSL_NAMESPACE::View<VSL_NAMESPACE::D2> image_view(swapchain);
    VSL_NAMESPACE::RenderPass pass(swapchain);

    _data2 = std::make_shared<AHO_NAMESPACE::window::WindowData>(AHO_NAMESPACE::window::WindowData{
            .surface = surface,
            .swapchain = swapchain,
            .image_view = image_view,
            .render_pass = pass,
            .frame_buffer = VSL_NAMESPACE::FrameBuffer<VSL_NAMESPACE::D2>(swapchain, image_view, pass),
            .image_available = _engine._data->synchro_manager
                    .createSemaphore(name + "ImageAvailable", _engine._data->command_manager.getBuffer().getSize()),
            .render_finished = _engine._data->synchro_manager
                    .createSemaphore(name + "RenderFinished", _engine._data->command_manager.getBuffer().getSize()),
            .in_flight = _engine._data->synchro_manager
                    .createFence(name + "InFlight", _engine._data->command_manager.getBuffer().getSize(), true)
    });
}

aho::window::Window::Window(engine::EngineAccessor engine, std::shared_ptr<vsl::Window::WindowData> vsl_data,
                            std::shared_ptr<aho::window::WindowData> aho_data)
        : VSL_NAMESPACE::Window(std::move(vsl_data)), engine(std::move(engine)) {
    _data2 = std::move(aho_data);
}

bool aho::window::Window::resize(int width, int height) {
    if (not VSL_NAMESPACE::PureWindow::resize(width, height))
        return false;
    _data2->swapchain = VSL_NAMESPACE::Swapchain(engine._data->logical_device, _data2->surface, _data2->swapchain);
    _data2->image_view = VSL_NAMESPACE::View<VSL_NAMESPACE::D2>(_data2->swapchain);
    _data2->frame_buffer = VSL_NAMESPACE::FrameBuffer<VSL_NAMESPACE::D2>(_data2->swapchain, _data2->image_view,
                                                                         _data2->render_pass);
    for (const auto&[_, p] : _data->plugins) {
        auto hook = std::dynamic_pointer_cast<WindowResizeHookPlugin>(p);
        if (not hook)
            continue;
        hook->hook(this);
    }
    return true;
}

void aho::window::Window::maximize() {
    glfwMaximizeWindow((GLFWwindow *) _data->window_handle);
}

void aho::window::Window::restore() {
    glfwRestoreWindow((GLFWwindow *) _data->window_handle);
}

bool aho::window::Window::set_title(const std::string &title) {
    glfwSetWindowTitle((GLFWwindow *) _data->window_handle, title.c_str());
    return true;
}

aho::d2::VectorI aho::window::Window::window_size() {
    d2::VectorI v;
    glfwGetWindowSize((GLFWwindow *) _data->window_handle, &v.value.x.value, &v.value.y.value);
    return v;
}

aho::d2::VectorI aho::window::Window::frame_size() {
    d2::VectorI v;
    glfwGetFramebufferSize((GLFWwindow *) _data->window_handle, &v.value.x.value, &v.value.y.value);
    return v;
}

aho::window::WindowResizeHookPlugin::WindowResizeHookPlugin(vsl::PureWindow *w,
                                                            std::function<void(aho::window::Window *)> hook)
        : hook(std::move(hook)) {
}

bool aho::window::Window::close() {
    _data2.reset();
    vsl::PureWindow::close();
}

aho::window::WindowData::~WindowData() {
    // vsl::loggingln("window destroy");
}
