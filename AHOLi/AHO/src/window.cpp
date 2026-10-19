#include <AHO/window.hpp>
#include <GLFW/glfw3.h>

#include <utility>

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::GetMonitorSize() {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
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
    _data2->swapchain = VSL_NAMESPACE::Swapchain(engine._data->logical_device, _data2->surface, _data2->swapchain);
    _data2->frame_buffer = VSL_NAMESPACE::FrameBuffer(_data2->swapchain, _data2->render_pass);
    if (_data2->render_finished.size() != _data2->swapchain.getSwapImageSize())
        _data2->render_finished = engine._data->synchro_manager.createSemaphore(
            name() + "RenderFinished", _data2->swapchain.getSwapImageSize());
    for (const auto& [_, p] : _data->plugins) {
        auto hook = std::dynamic_pointer_cast<WindowResizeHookPlugin>(p);
        if (not hook)
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
