#include <AHO/window.hpp>
#include "window_impl.hpp"
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <GLFW/glfw3.h>
#include <VSL/exceptions.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#ifdef __APPLE__
#include "window_macos.h"
#endif

#include <algorithm>
#include <utility>
#include <vector>

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::get_monitor_size() {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Primary monitor is not available");
#ifdef __APPLE__
#if defined(GLFW_PLATFORM_COCOA)
    if (glfwGetPlatform() == GLFW_PLATFORM_COCOA) {
#endif
        int width, height;
        if (!aho_get_cocoa_monitor_size(monitor, &width, &height))
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Primary monitor video mode is not available");
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
    thread_local std::vector<std::shared_ptr<AHO_NAMESPACE::window::WindowData>> CLOSING_WINDOWS;

    const std::string& checked_window_name(const std::string& name, const AHO_NAMESPACE::engine::EngineAccessor& engine) {
        const auto& data = engine.data();
        if (!data || !data->vulkan_instance.accessor || !data->logical_device.data_ ||
            !data->command_manager.data_ || !data->synchro_manager.data_)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
        auto buffers = data->command_manager.get_buffer();
        if (!buffers.data_ || buffers.get_size() == 0)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
        return name;
    }
} // namespace

std::shared_ptr<VSL_NAMESPACE::Surface> AHO_NAMESPACE::window::detail::make_window_surface(
    VSL_NAMESPACE::Window& window, VSL_NAMESPACE::VulkanAccessor instance) {
    using namespace VSL_NAMESPACE;
    const auto data = window.data_;
    const auto os = window.window_owner();
    const std::array requests{storage::StartupRequest{{0}, storage::StartupKind::SURFACE}};
    const storage::StartupPlan<1> plan(requests);
    const std::array<lifetime::StartupInit, 1> inputs{lifetime::SurfaceInit{std::move(instance), os}};
    lifetime::StartupArena arena(plan, inputs);
    if (!data || !data->window_handle || window.data_ != data)
        throw exceptions::RuntimeException("Window", "Window closed during Surface creation");
    return window.add_plugin<Surface>(arena.surface({0}));
}

std::shared_ptr<AHO_NAMESPACE::window::WindowData> AHO_NAMESPACE::window::detail::make_window_data(
    const engine::EngineAccessor& engine, std::shared_ptr<VSL_NAMESPACE::Surface> surface, const std::string& name) {
    const auto engine_data = engine.data();
    auto& data = *engine_data;
    VSL_NAMESPACE::Swapchain swapchain(data.logical_device, surface);
    VSL_NAMESPACE::RenderPass pass(swapchain);
    const auto frames = data.command_manager.get_buffer().get_size();
    return std::make_shared<WindowData>(WindowData{
        .surface = std::move(surface),
        .swapchain = swapchain,
        .render_pass = pass,
        .frame_buffer = VSL_NAMESPACE::FrameBuffer(swapchain, pass),
        .image_available = data.synchro_manager.create_semaphore(name + "ImageAvailable", frames),
        .render_finished = swapchain.finished_semaphores(),
        .in_flight = data.synchro_manager.create_fence(name + "InFlight", frames, true)});
}

AHO_NAMESPACE::engine::EngineAccessor AHO_NAMESPACE::window::Window::DefaultEngine() {
    auto current = AHO_NAMESPACE::engine::DefaultEngineRegistration::Current();
    if (!current.data())
        throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
    return current;
}

AHO_NAMESPACE::window::Window::Window(std::string name, int width, int height, AHO_NAMESPACE::engine::EngineAccessor engine_)
    : VSL_NAMESPACE::Window(checked_window_name(name, engine_), width, height) {
    const auto data = data_;
    const auto os = window_owner();
    try {
        engine = engine_;
        auto surface = detail::make_window_surface(*this, engine_.data()->vulkan_instance);
        auto drawing = detail::make_window_data(engine, std::move(surface), name);
        if (data_ != data || !data->window_handle || data->window_handle != os.native_handle())
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Window closed during drawing resource creation");
        data2 = std::move(drawing);
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
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    check_recreation_allowed();
    const auto data = data_;
    const auto drawing = data2;
    if (not VSL_NAMESPACE::PureWindow::resize(width, height))
        return false;
    if (!data->window_handle || data_ != data || data2 != drawing)
        return false;
    return recreate_swapchain();
}

void AHO_NAMESPACE::window::Window::check_recreation_allowed() {
    if (!*this || !data2 || !engine.data())
        throw std::logic_error("Swapchain recreation requires a live AHO window");
    if (std::find(RECREATING_WINDOWS.begin(), RECREATING_WINDOWS.end(), data2) != RECREATING_WINDOWS.end())
        throw std::logic_error("Swapchain recreation is already in progress");
    if (auto* phase = AHO_NAMESPACE::DrawPhase::CurrentForManager(engine.data()->command_manager);
        phase && phase->state == VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT)
        throw std::logic_error("Cannot recreate a swapchain while recording a DrawPhase");
}

bool AHO_NAMESPACE::window::Window::recreate_swapchain(VSL_NAMESPACE::ImageType additional_usage) {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    check_recreation_allowed();
    const auto data = data_;
    const auto drawing = data2;
    const auto runtime = engine.data();
    auto manager = runtime->command_manager;
    const auto os = window_owner();
    // callbackがwindowを閉じても、共有状態を生存させて確実にガードを解除する。
    RECREATING_WINDOWS.push_back(drawing);
    struct RecreationGuard {
        ~RecreationGuard() { RECREATING_WINDOWS.pop_back(); }
    } guard;
    manager.wait_idle();
    if (!data->window_handle || data_ != data || data2 != drawing)
        return false;
    const auto usage = drawing->swapchain.images.at(0).get_usage() | additional_usage;
    std::optional<VSL_NAMESPACE::SwapchainAccessor> previous;
    if (drawing->swapchain.state() == VSL_NAMESPACE::SwapchainState::ACTIVE)
        previous = drawing->swapchain;
    const VSL_NAMESPACE::SwapchainCreateInfo info{.usage = usage, .present_fences = drawing->swapchain.present_fences()};
    VSL_NAMESPACE::lifetime::SwapchainArena candidate(runtime->logical_device, *drawing->surface, info, std::move(previous));
    auto next = std::move(candidate).view();
    if (!data->window_handle || data_ != data || data2 != drawing)
        return false;
    auto pass = drawing->render_pass;
    if (pass.description().attachments.front().format.type() != next.get_format().type())
        pass = VSL_NAMESPACE::RenderPass(next);
    auto frames = VSL_NAMESPACE::FrameBuffer(next, pass);
    auto finished = next.finished_semaphores();
    if (!data->window_handle || data_ != data || data2 != drawing)
        return false;
    drawing->swapchain = std::move(next);
    drawing->render_pass = std::move(pass);
    drawing->frame_buffer = std::move(frames);
    drawing->render_finished = std::move(finished);
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
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    const auto os = window_owner();
    glfwMaximizeWindow(static_cast<GLFWwindow*>(os.native_handle()));
}

void AHO_NAMESPACE::window::Window::restore() {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    const auto os = window_owner();
    glfwRestoreWindow(static_cast<GLFWwindow*>(os.native_handle()));
}

bool AHO_NAMESPACE::window::Window::set_title(const std::string& title) {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    const auto os = window_owner();
    glfwSetWindowTitle(static_cast<GLFWwindow*>(os.native_handle()), title.c_str());
    return true;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::window_size() {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    const auto os = window_owner();
    d2::VectorI v;
    glfwGetWindowSize(static_cast<GLFWwindow*>(os.native_handle()), &v.value.x.value, &v.value.y.value);
    return v;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::frame_size() {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    const auto os = window_owner();
    d2::VectorI v;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(os.native_handle()), &v.value.x.value, &v.value.y.value);
    return v;
}

AHO_NAMESPACE::window::WindowResizeHookPlugin::WindowResizeHookPlugin(VSL_NAMESPACE::PureWindow* w,
                                                                      std::function<void(AHO_NAMESPACE::window::Window*)> hook)
    : hook(std::move(hook)) {
}

bool AHO_NAMESPACE::window::Window::close() {
    if (!data_ || (!data_->window_handle && !data2) || (data2 && std::ranges::contains(CLOSING_WINDOWS, data2)))
        return false;
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    const auto data = data_;
    const auto drawing = data2;
    const auto runtime = engine.data();
    if (!data || (!data->window_handle && !drawing) || (drawing && std::ranges::contains(CLOSING_WINDOWS, drawing)))
        return false;
    if (drawing && !runtime)
        throw std::logic_error("Closing graphics requires its engine");
    if (drawing)
        CLOSING_WINDOWS.push_back(drawing);
    struct ClosingGuard {
        bool active;
        ~ClosingGuard() {
            if (active)
                CLOSING_WINDOWS.pop_back();
        }
    } guard{bool(drawing)};
    const auto closed = VSL_NAMESPACE::PureWindow::close();
    if (drawing) {
        auto manager = runtime->command_manager;
        manager.wait_idle();
    }
    if (data_ == data && data2 == drawing)
        data2.reset();
    return closed;
}

AHO_NAMESPACE::window::WindowData::~WindowData() {
}
