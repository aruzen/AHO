#include <AHO/window.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <GLFW/glfw3.h>
#include <VSL/exceptions.hpp>
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
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
    thread_local std::vector<std::shared_ptr<AHO_NAMESPACE::window::WindowData>> RECREATING_WINDOWS;

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
    check_recreation_allowed();
    if (not VSL_NAMESPACE::PureWindow::resize(width, height))
        return false;
    return recreate_swapchain();
}

void AHO_NAMESPACE::window::Window::check_recreation_allowed() {
    if (!*this || !data2 || !engine.data_)
        throw std::logic_error("Swapchain recreation requires a live AHO window");
    if (std::find(RECREATING_WINDOWS.begin(), RECREATING_WINDOWS.end(), data2) != RECREATING_WINDOWS.end())
        throw std::logic_error("Swapchain recreation is already in progress");
    if (auto* phase = AHO_NAMESPACE::DrawPhase::CurrentForManager(engine.data_->command_manager);
        phase && phase->state == VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT)
        throw std::logic_error("Cannot recreate a swapchain while recording a DrawPhase");
}

bool AHO_NAMESPACE::window::Window::recreate_swapchain(VSL_NAMESPACE::ImageType additional_usage) {
    check_recreation_allowed();
    // callbackがwindowを閉じても、共有状態を生存させて確実にガードを解除する。
    RECREATING_WINDOWS.push_back(data2);
    struct RecreationGuard {
        ~RecreationGuard() { RECREATING_WINDOWS.pop_back(); }
    } guard;
    engine.data_->command_manager.wait_idle();
    const auto usage = data2->swapchain.images.at(0).get_usage() | additional_usage;
    auto next = VSL_NAMESPACE::Swapchain(engine.data_->logical_device, data2->surface, data2->swapchain,
                                         std::nullopt, std::nullopt, usage);
    auto pass = data2->render_pass;
    if (pass.description().attachments.front().format.type() != next.get_format().type())
        pass = VSL_NAMESPACE::RenderPass(next);
    auto frames = VSL_NAMESPACE::FrameBuffer(next, pass);
    auto finished = data2->render_finished;
    if (finished.size() != next.get_swap_image_size())
        finished = engine.data_->synchro_manager.create_semaphore(name() + "RenderFinished", next.get_swap_image_size());
    data2->swapchain = std::move(next);
    data2->render_pass = std::move(pass);
    data2->frame_buffer = std::move(frames);
    data2->render_finished = std::move(finished);
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
