#include <AHO/window.hpp>
#include <GLFW/glfw3.h>
#include <VSL/exceptions.hpp>
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif

#include <utility>
#include <vector>

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::get_monitor_size() {
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

namespace {
    const std::string& checked_window_name(const std::string& name, const AHO_NAMESPACE::engine::EngineAccessor& engine) {
        const auto& data = engine.data_;
        if (!data || !data->vulkan_instance.accessor || !data->logical_device.data_ ||
            !data->command_manager.data_ || !data->synchro_manager.data_)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
        auto buffers = data->command_manager.get_buffer();
        if (!buffers.data_ || buffers.get_size() == 0)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
        return name;
    }
} // namespace

AHO_NAMESPACE::engine::EngineAccessor AHO_NAMESPACE::window::Window::DefaultEngine() {
    auto* current = AHO_NAMESPACE::engine::EngineAccessor::Instance();
    if (!current)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
    return *current;
}

AHO_NAMESPACE::window::Window::Window(std::string name, int width, int height, AHO_NAMESPACE::engine::EngineAccessor engine_)
    : VSL_NAMESPACE::Window(checked_window_name(name, engine_), width, height) {
    try {
        engine = engine_;
        auto surface = this->add_plugin<VSL_NAMESPACE::Surface>(engine_.data_->vulkan_instance);
        VSL_NAMESPACE::Swapchain swapchain(engine_.data_->logical_device, surface);
        VSL_NAMESPACE::RenderPass pass(swapchain);

        data2 = std::make_shared<AHO_NAMESPACE::window::WindowData>(AHO_NAMESPACE::window::WindowData{
            .surface = surface,
            .swapchain = swapchain,
            .render_pass = pass,
            .frame_buffer = VSL_NAMESPACE::FrameBuffer(swapchain, pass),
            .image_available = engine_.data_->synchro_manager
                                   .create_semaphore(name + "ImageAvailable", engine_.data_->command_manager.get_buffer().get_size()),
            .render_finished = engine_.data_->synchro_manager
                                   .create_semaphore(name + "RenderFinished", swapchain.get_swap_image_size()),
            .in_flight = engine_.data_->synchro_manager
                             .create_fence(name + "InFlight", engine_.data_->command_manager.get_buffer().get_size(), true)});
    } catch (...) {
        data_->plugins.clear();
        VSL_NAMESPACE::PureWindow::close();
        throw;
    }
}

AHO_NAMESPACE::window::Window::Window(engine::EngineAccessor engine, std::shared_ptr<VSL_NAMESPACE::Window::WindowData> vsl_data,
                                      std::shared_ptr<AHO_NAMESPACE::window::WindowData> aho_data)
    : VSL_NAMESPACE::Window(std::move(vsl_data)), engine(std::move(engine)) {
    data2 = std::move(aho_data);
}

bool AHO_NAMESPACE::window::Window::resize(int width, int height) {
    if (not VSL_NAMESPACE::PureWindow::resize(width, height))
        return false;
    engine.data_->command_manager.wait_idle();
    const auto usage = data2->swapchain.images.at(0).get_usage();
    data2->swapchain = VSL_NAMESPACE::Swapchain(engine.data_->logical_device, data2->surface, data2->swapchain,
                                                 std::nullopt, std::nullopt, usage);
    data2->frame_buffer = VSL_NAMESPACE::FrameBuffer(data2->swapchain, data2->render_pass);
    if (data2->render_finished.size() != data2->swapchain.get_swap_image_size())
        data2->render_finished = engine.data_->synchro_manager.create_semaphore(
            name() + "RenderFinished", data2->swapchain.get_swap_image_size());
    const auto data = data_;
    std::vector<std::pair<unsigned char, std::weak_ptr<WindowResizeHookPlugin>>> hooks;
    hooks.reserve(data->plugins.size());
    for (const auto& [id, plugin] : data->plugins)
        if (auto hook = std::dynamic_pointer_cast<WindowResizeHookPlugin>(plugin))
            hooks.emplace_back(id, hook);
    for (const auto& [id, candidate] : hooks) {
        if (data_ != data || !data2 || !data->window_handle)
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
    glfwMaximizeWindow((GLFWwindow*)data_->window_handle);
}

void AHO_NAMESPACE::window::Window::restore() {
    glfwRestoreWindow((GLFWwindow*)data_->window_handle);
}

bool AHO_NAMESPACE::window::Window::set_title(const std::string& title) {
    glfwSetWindowTitle((GLFWwindow*)data_->window_handle, title.c_str());
    return true;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::window_size() {
    d2::VectorI v;
    glfwGetWindowSize((GLFWwindow*)data_->window_handle, &v.value.x.value, &v.value.y.value);
    return v;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::frame_size() {
    d2::VectorI v;
    glfwGetFramebufferSize((GLFWwindow*)data_->window_handle, &v.value.x.value, &v.value.y.value);
    return v;
}

AHO_NAMESPACE::window::WindowResizeHookPlugin::WindowResizeHookPlugin(VSL_NAMESPACE::PureWindow* w,
                                                                      std::function<void(AHO_NAMESPACE::window::Window*)> hook)
    : hook(std::move(hook)) {
}

bool AHO_NAMESPACE::window::Window::close() {
    if (data2)
        engine.data_->command_manager.wait_idle();
    data2.reset();
    return VSL_NAMESPACE::PureWindow::close();
}

AHO_NAMESPACE::window::WindowData::~WindowData() {
}
