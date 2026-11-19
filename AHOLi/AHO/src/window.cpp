#include "engine/engine_access.hpp"
namespace {
    using AHO_NAMESPACE::engine::detail::EngineAccess;
}
#include <AHO/window.hpp>
#include "window_impl.hpp"
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/surface_recovery.hpp>
#include <VSL/window_plugin.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <GLFW/glfw3.h>
#include <VSL/exceptions.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#ifdef __APPLE__
#include "window_macos.h"
#endif

#include <algorithm>
#include <exception>
#include <type_traits>
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
    struct RetiredWindowResources {
        std::shared_ptr<VSL_NAMESPACE::Surface> surface;
        VSL_NAMESPACE::SwapchainAccessor swapchain;
        VSL_NAMESPACE::RenderPassAccessor pass;
        VSL_NAMESPACE::FrameBufferAccessor frames;
        VSL_NAMESPACE::SemaphoreHolder available, finished;
        VSL_NAMESPACE::FenceHolder flight, acquired;
        std::shared_ptr<AHO_NAMESPACE::drawer::detail::StandardDrawState> cache;
        std::optional<AHO_NAMESPACE::window::WindowGenerationCandidate> pending;
        explicit RetiredWindowResources(AHO_NAMESPACE::window::WindowData& data) noexcept;
    };
    struct GraphicsClose : VSL_NAMESPACE::PureWindow::Plugin, VSL_NAMESPACE::PureWindow::OnUpdate {
        VSL_NAMESPACE::WeakWindow native;
        std::weak_ptr<AHO_NAMESPACE::window::WindowData> drawing;
        VSL_NAMESPACE::WeakLogicalDevice device;
        VSL_NAMESPACE::WeakSurface surface;
        GraphicsClose(VSL_NAMESPACE::PureWindow* window, std::shared_ptr<AHO_NAMESPACE::window::WindowData> drawing,
                      VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::Surface surface);
        bool on_update(VSL_NAMESPACE::PureWindow window) override;
    };
    thread_local const AHO_NAMESPACE::window::detail::FrameBufferQuery* ACTIVE_FRAME_QUERY = nullptr;
    constexpr std::array<VSL_NAMESPACE::typed::ResourceRequest, 3> WINDOW_SYNC_REQUESTS{{{{0}, VSL_NAMESPACE::storage::ResourceKind::SEMAPHORE},
                                                                                         {{1}, VSL_NAMESPACE::storage::ResourceKind::FENCE},
                                                                                         {{2}, VSL_NAMESPACE::storage::ResourceKind::FENCE}}};
    constexpr VSL_NAMESPACE::typed::ResourcePlan<3> WINDOW_SYNC_PLAN(WINDOW_SYNC_REQUESTS);

    RetiredWindowResources::RetiredWindowResources(AHO_NAMESPACE::window::WindowData& data) noexcept
        : surface(std::exchange(data.surface, {})), swapchain(std::move(data.swapchain)),
          pass(std::move(data.render_pass)), frames(std::move(data.frame_buffer)),
          available(std::move(data.image_available)), finished(std::move(data.render_finished)),
          flight(std::move(data.in_flight)), acquired(std::move(data.acquire_fences)),
          cache(std::exchange(data.standard_draw, {})), pending(std::exchange(data.pending_generation, std::nullopt)) {
        data.swapchain = {};
        data.deferred_usage.reset();
    }
    GraphicsClose::GraphicsClose(VSL_NAMESPACE::PureWindow* window, std::shared_ptr<AHO_NAMESPACE::window::WindowData> drawing,
                                 VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::Surface surface)
        : native(window->weak()), drawing(drawing), device(device.weak()), surface(surface.weak()) {}
    bool GraphicsClose::on_update(VSL_NAMESPACE::PureWindow window) {
        const auto data = native.lock();
        const auto target = drawing.lock();
        if (!data || data != window || (target && target->closed))
            return true;
        if (!window.should_close())
            return true;
        const auto parent_device = device.lock();
        const auto parent_surface = surface.lock();
        if (!target) {
            if (parent_device && parent_surface)
                VSL_NAMESPACE::lifetime::queue_surface_retirement(parent_device, parent_surface);
            window.close();
            if (parent_device)
                (void)VSL_NAMESPACE::lifetime::collect_surface_retirements(parent_device, VSL_NAMESPACE::lifetime::SurfaceRecoveryMode::WAIT);
            return true;
        }
        if (!parent_device || !parent_surface)
            throw std::logic_error("Window close lost its original parents");
        AHO_NAMESPACE::window::detail::WindowClose::Run(data, target, parent_device, parent_surface);
        return true;
    }

    const std::string& checked_window_name(const std::string& name, const AHO_NAMESPACE::engine::EngineAccessor& engine) {
        const auto& data = EngineAccess::State(engine);
        if (!data || !data->logical_device ||
            !data->command_manager)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
        auto buffers = data->command_manager.get_buffer();
        if (!buffers || buffers.get_size() == 0)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
        return name;
    }
    bool matches_request(const VSL_NAMESPACE::SwapchainCreateInfo& a, const VSL_NAMESPACE::SwapchainCreateInfo& b) noexcept {
        return a.width == b.width && a.height == b.height && a.usage == b.usage && a.present_fences == b.present_fences;
    }
    bool replaceable(const VSL_NAMESPACE::SwapchainAccessor& chain) {
        return chain.state() == VSL_NAMESPACE::SwapchainState::ACTIVE || chain.state() == VSL_NAMESPACE::SwapchainState::RECOVERY_REQUIRED;
    }
    void replace_candidate(std::optional<AHO_NAMESPACE::window::WindowGenerationCandidate>& current,
                           AHO_NAMESPACE::window::WindowGenerationCandidate next) noexcept {
        auto replacement = std::make_optional(std::move(next));
        static_assert(std::is_nothrow_swappable_v<decltype(current)>);
        current.swap(replacement);
    }
    VSL_NAMESPACE::ImageType requested_usage(const AHO_NAMESPACE::window::WindowData& drawing, VSL_NAMESPACE::ImageType additional) {
        return (drawing.pending_generation ? drawing.pending_generation->request.usage : drawing.swapchain.image(0).get_usage()) |
               drawing.deferred_usage.value_or(VSL_NAMESPACE::ImageType::NONE) | additional;
    }
    VSL_NAMESPACE::ImageType validated_recreation_usage(const AHO_NAMESPACE::window::WindowData& drawing, VSL_NAMESPACE::ImageType additional) {
        const auto usage = requested_usage(drawing, additional);
        const auto chain = drawing.swapchain;
        chain.require_image_usage(usage);
        return usage;
    }
} // namespace

AHO_NAMESPACE::window::detail::ScopedFrameBufferQuery::ScopedFrameBufferQuery(FrameBufferQuery query) noexcept
    : _query(query), _previous(std::exchange(ACTIVE_FRAME_QUERY, &_query)) {}

AHO_NAMESPACE::window::detail::ScopedFrameBufferQuery::~ScopedFrameBufferQuery() {
    ACTIVE_FRAME_QUERY = _previous;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::detail::frame_buffer_size(void* native) {
    const auto query = ACTIVE_FRAME_QUERY ? *ACTIVE_FRAME_QUERY : FrameBufferQuery{};
    d2::VectorI size;
    if (query.get_size)
        query.get_size(query.context, native, &size.value.x.value, &size.value.y.value);
    else
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(native), &size.value.x.value, &size.value.y.value);
    if (size.value.x.value < 0 || size.value.y.value < 0)
        throw std::runtime_error("Window framebuffer extent is negative");
    return size;
}

std::shared_ptr<VSL_NAMESPACE::Surface> AHO_NAMESPACE::window::detail::make_window_surface(
    VSL_NAMESPACE::PureWindow window, VSL_NAMESPACE::VulkanAccessor instance) {
    using namespace VSL_NAMESPACE;
    const auto os = window.window_owner();
    const std::array requests{storage::StartupRequest{{0}, storage::StartupKind::SURFACE}};
    const storage::StartupPlan<1> plan(requests);
    const std::array<lifetime::StartupInit, 1> inputs{lifetime::SurfaceInit{std::move(instance), os}};
    lifetime::StartupArena arena(plan, inputs);
    if (!window)
        throw exceptions::RuntimeException("Window", "Window closed during Surface creation");
    return window.add_plugin<Surface>(arena.surface({0}));
}

std::shared_ptr<AHO_NAMESPACE::window::WindowData> AHO_NAMESPACE::window::detail::make_window_data(
    const engine::EngineAccessor& engine, std::shared_ptr<VSL_NAMESPACE::Surface> surface,
    std::optional<VSL_NAMESPACE::PresentFencePolicy> present_fences) {
    const auto engine_data = EngineAccess::State(engine);
    auto& data = *engine_data;
    const VSL_NAMESPACE::SwapchainCreateInfo info{.present_fences = present_fences.value_or(data.present_fences)};
    auto swapchain = VSL_NAMESPACE::lifetime::SwapchainArena(data.logical_device, *surface, info).view();
    VSL_NAMESPACE::RenderPass pass(swapchain);
    const auto frames = data.command_manager.get_buffer().get_size();
    const std::array<VSL_NAMESPACE::lifetime::ResourceInit, 3> inputs{
        VSL_NAMESPACE::SemaphoreCreateInfo{frames},
        VSL_NAMESPACE::FenceCreateInfo{frames, true},
        VSL_NAMESPACE::FenceCreateInfo{frames, false}};
    const VSL_NAMESPACE::lifetime::ResourceArena synchronization(data.logical_device, WINDOW_SYNC_PLAN, inputs);
    return std::make_shared<WindowData>(WindowData{
        .surface = std::move(surface),
        .swapchain = swapchain,
        .render_pass = pass,
        .frame_buffer = VSL_NAMESPACE::FrameBuffer(swapchain, pass),
        .image_available = synchronization.semaphore({0}),
        .render_finished = swapchain.finished_semaphores(),
        .in_flight = synchronization.fence({1}),
        .acquire_fences = synchronization.fence({2})});
}

AHO_NAMESPACE::engine::EngineAccessor AHO_NAMESPACE::window::Window::DefaultEngine() {
    auto current = AHO_NAMESPACE::engine::DefaultEngineRegistration::Current();
    if (!EngineAccess::State(current))
        throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "A booted engine is required");
    return current;
}

AHO_NAMESPACE::window::Window::Window(std::string name, int width, int height, AHO_NAMESPACE::engine::EngineAccessor engine_,
                                      std::optional<VSL_NAMESPACE::PresentFencePolicy> present_fences)
    : VSL_NAMESPACE::Window(checked_window_name(name, engine_), width, height) {
    Call call(*this);
    VSL_NAMESPACE::PureWindow native = *this;
    const auto os = window_owner();
    try {
        engine = engine_;
        auto surface = detail::make_window_surface(native, engine_.instance());
        if (!call.active() || !native || native != *this)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Window changed during Surface creation");
        auto drawing = detail::make_window_data(engine_, std::move(surface), present_fences);
        if (!call.active() || !native || native != *this)
            throw VSL_NAMESPACE::exceptions::RuntimeException("Window", "Window closed during drawing resource creation");
        data2 = std::move(drawing);
        detail::WindowClose::Install(*this);
    } catch (...) {
        native.close();
        throw;
    }
}

AHO_NAMESPACE::window::Window::Window(engine::EngineAccessor engine, VSL_NAMESPACE::PureWindow vsl_window,
                                      std::shared_ptr<AHO_NAMESPACE::window::WindowData> aho_data)
    : VSL_NAMESPACE::Window(std::move(vsl_window)), engine(std::move(engine)) {
    data2 = std::move(aho_data);
    detail::WindowClose::Install(*this);
}

bool AHO_NAMESPACE::window::Window::resize(int width, int height) {
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Window size must be positive");
    Call call(*this);
    check_recreation_allowed();
    VSL_NAMESPACE::PureWindow native = *this;
    const auto drawing = data2;
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    if (!call.active() || !native || native != *this || data2 != drawing)
        return false;
    if (not VSL_NAMESPACE::PureWindow::resize(width, height))
        return false;
    if (!call.active() || !native || native != *this || data2 != drawing)
        return false;
    return recreate_swapchain();
}

void AHO_NAMESPACE::window::Window::check_recreation_allowed() {
    if (!*this || !data2 || data2->closed || !EngineAccess::State(engine))
        throw std::logic_error("Swapchain recreation requires a live AHO window");
    if (std::find(RECREATING_WINDOWS.begin(), RECREATING_WINDOWS.end(), data2) != RECREATING_WINDOWS.end())
        throw std::logic_error("Swapchain recreation is already in progress");
    if (auto* phase = AHO_NAMESPACE::DrawPhase::CurrentForManager(EngineAccess::State(engine)->command_manager);
        phase && phase->state == VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT)
        throw std::logic_error("Cannot recreate a swapchain while recording a DrawPhase");
}

bool AHO_NAMESPACE::window::Window::recreate_swapchain(VSL_NAMESPACE::ImageType additional_usage) {
    Call call(*this);
    check_recreation_allowed();
    VSL_NAMESPACE::PureWindow native = *this;
    const auto drawing = data2;
    const auto runtime = EngineAccess::State(engine);
    auto manager = runtime->command_manager;
    const auto os = window_owner();
    const auto current = [this, &call, &native, &drawing] {
        return call.active() && native && native == *this && data2 == drawing && !drawing->closed;
    };
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    if (!current())
        return false;
    // callbackがwindowを閉じても、共有状態を生存させて確実にガードを解除する。
    RECREATING_WINDOWS.push_back(drawing);
    struct RecreationGuard {
        ~RecreationGuard() { RECREATING_WINDOWS.pop_back(); }
    } guard;
    const auto before = detail::frame_buffer_size(os.native_handle());
    if (!current())
        return false;
    if (before.value.x.value == 0 || before.value.y.value == 0) {
        const auto usage = validated_recreation_usage(*drawing, additional_usage);
        if (current())
            drawing->deferred_usage = usage;
        return false;
    }
    manager.wait_idle();
    if (!current())
        return false;
    const auto size = detail::frame_buffer_size(os.native_handle());
    if (!current())
        return false;
    if (size.value.x.value == 0 || size.value.y.value == 0) {
        const auto usage = validated_recreation_usage(*drawing, additional_usage);
        if (current())
            drawing->deferred_usage = usage;
        return false;
    }
    const auto width = size.value.x.value, height = size.value.y.value;
    const auto usage = requested_usage(*drawing, additional_usage);
    const VSL_NAMESPACE::SwapchainCreateInfo info{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height), .usage = usage, .present_fences = drawing->swapchain.present_fences()};
    auto& pending = drawing->pending_generation;
    if (!pending || !pending->swapchain || pending->swapchain->state() != VSL_NAMESPACE::SwapchainState::ACTIVE || !matches_request(pending->request, info)) {
        std::optional<VSL_NAMESPACE::SwapchainAccessor> previous;
        if (pending && pending->swapchain && replaceable(*pending->swapchain))
            previous = pending->swapchain;
        else if (replaceable(drawing->swapchain))
            previous = drawing->swapchain;
        VSL_NAMESPACE::SwapchainCreateProgress progress;
        try {
            VSL_NAMESPACE::lifetime::SwapchainArena candidate(runtime->logical_device, *drawing->surface, info, std::move(previous), {}, &progress);
            if (!current())
                return false;
            replace_candidate(pending, WindowGenerationCandidate{info, std::move(candidate).view()});
            if (!current())
                return false;
        } catch (...) {
            if (progress.native_started && current()) {
                replace_candidate(pending, WindowGenerationCandidate{info, std::nullopt});
                if (current())
                    drawing->deferred_usage.reset();
            }
            throw;
        }
    }
    if (!current())
        return false;
    drawing->deferred_usage.reset();
    auto next = *pending->swapchain;
    auto pass = drawing->render_pass;
    if (pass.description().attachments.front().format.type() != next.get_format().type())
        pass = VSL_NAMESPACE::RenderPass(next);
    if (!current())
        return false;
    auto frames = VSL_NAMESPACE::FrameBuffer(next, pass);
    if (!current())
        return false;
    auto finished = next.finished_semaphores();
    if (!current())
        return false;
    const auto hooks = native.plugin_snapshot();
    // 旧ownerは全fieldの交換が済んでから解放し、破棄時の再入でも混合世代を見せない。
    using std::swap;
    static_assert(std::is_nothrow_swappable_v<decltype(next)> && std::is_nothrow_swappable_v<decltype(pass)> &&
                  std::is_nothrow_swappable_v<decltype(frames)> && std::is_nothrow_swappable_v<decltype(finished)>);
    swap(drawing->swapchain, next);
    swap(drawing->render_pass, pass);
    swap(drawing->frame_buffer, frames);
    swap(drawing->render_finished, finished);
    pending.reset();
    for (const auto& [id, candidate] : hooks) {
        if (!current())
            break;
        const auto owner = candidate.lock();
        if (!native.contains_plugin(id, owner))
            continue;
        if (auto* hook = dynamic_cast<WindowResizeHookPlugin*>(owner.get())) {
            const auto callback = hook->hook;
            if (callback)
                callback(this);
        }
    }
    return true;
}

void AHO_NAMESPACE::window::Window::maximize() {
    const auto os = window_owner();
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    glfwMaximizeWindow(static_cast<GLFWwindow*>(os.native_handle()));
}

void AHO_NAMESPACE::window::Window::restore() {
    const auto os = window_owner();
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    glfwRestoreWindow(static_cast<GLFWwindow*>(os.native_handle()));
}

bool AHO_NAMESPACE::window::Window::set_title(const std::string& title) {
    const auto os = window_owner();
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    glfwSetWindowTitle(static_cast<GLFWwindow*>(os.native_handle()), title.c_str());
    return true;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::window_size() {
    const auto os = window_owner();
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    d2::VectorI v;
    glfwGetWindowSize(static_cast<GLFWwindow*>(os.native_handle()), &v.value.x.value, &v.value.y.value);
    return v;
}

AHO_NAMESPACE::d2::VectorI AHO_NAMESPACE::window::Window::frame_size() const {
    const auto os = window_owner();
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    return detail::frame_buffer_size(os.native_handle());
}

AHO_NAMESPACE::window::WindowResizeHookPlugin::WindowResizeHookPlugin(VSL_NAMESPACE::PureWindow* w,
                                                                      std::function<void(AHO_NAMESPACE::window::Window*)> hook, std::optional<unsigned char> requested_id)
    : Plugin(requested_id), hook(std::move(hook)) {
}

bool AHO_NAMESPACE::window::Window::close() {
    Call call(*this);
    VSL_NAMESPACE::PureWindow native = *this;
    const auto drawing = data2;
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    if (!call.active() || (!native && !drawing))
        return false;
    if (!drawing)
        return native.close();
    if (drawing->closed) {
        data2.reset();
        return false;
    }
    auto device = drawing->swapchain.get_device();
    auto surface = drawing->swapchain.get_surface();
    VSL_NAMESPACE::lifetime::queue_surface_retirement(device, surface);
    data2.reset();
    return detail::WindowClose::Run(native, drawing, std::move(device), std::move(surface));
}

bool AHO_NAMESPACE::window::detail::WindowClose::Run(VSL_NAMESPACE::PureWindow native,
                                                     std::shared_ptr<WindowData> drawing, VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                     VSL_NAMESPACE::Surface surface) {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    if (!drawing || drawing->closed)
        return false;
    VSL_NAMESPACE::lifetime::queue_surface_retirement(device, surface);
    drawing->closed = true;
    const RetiredWindowResources retired(*drawing);
    const auto closed = native.close();
    std::exception_ptr failure;
    try {
        CancelPhases(native, drawing);
    } catch (...) {
        failure = std::current_exception();
    }
    try {
        (void)VSL_NAMESPACE::lifetime::collect_surface_retirements(std::move(device), VSL_NAMESPACE::lifetime::SurfaceRecoveryMode::WAIT);
    } catch (...) {
        if (!failure)
            failure = std::current_exception();
    }
    if (failure)
        std::rethrow_exception(failure);
    return closed;
}

void AHO_NAMESPACE::window::detail::WindowClose::CancelPhases(const VSL_NAMESPACE::PureWindow& native,
                                                              const std::shared_ptr<WindowData>& drawing) {
    std::exception_ptr failure;
    for (;;) {
        auto* phase = AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE;
        while (phase && (phase->_close_requested || phase->_window != native || phase->_window.data2 != drawing))
            phase = phase->_previous_phase;
        if (!phase)
            break;
        phase->_close_requested = true;
        try {
            phase->VSL_NAMESPACE::DefaultPhase::cancel();
        } catch (...) {
            if (!failure)
                failure = std::current_exception();
        }
    }
    if (failure)
        std::rethrow_exception(failure);
}

void AHO_NAMESPACE::window::detail::WindowClose::Install(Window& window) {
    if (!window || !window.data2 || window.data2->closed)
        return;
    std::shared_ptr<VSL_NAMESPACE::window_plugin::QuietClose> previous;
    for (const auto& [id, candidate] : window.plugin_snapshot())
        if (auto close = std::dynamic_pointer_cast<VSL_NAMESPACE::window_plugin::QuietClose>(candidate.lock())) {
            previous = std::move(close);
            break;
        }
    for (const auto& [id, candidate] : window.plugin_snapshot())
        if (const auto close = std::dynamic_pointer_cast<GraphicsClose>(candidate.lock()); close && close->drawing.lock() == window.data2) {
            if (previous)
                window.remove_plugin(previous);
            return;
        }
    window.add_plugin<GraphicsClose>(window.data2, window.data2->swapchain.get_device(), window.data2->swapchain.get_surface());
    if (previous)
        window.remove_plugin(previous);
}

AHO_NAMESPACE::window::WindowData::~WindowData() {
}
