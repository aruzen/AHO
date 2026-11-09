#pragma once
#include <AHO/window.hpp>

namespace AHO_NAMESPACE::window::detail {
    struct WindowClose {
        static bool Run(std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData> native, std::shared_ptr<WindowData> drawing,
                        VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::Surface surface);
        static void CancelPhases(const std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData>& native, const std::shared_ptr<WindowData>& drawing);
        static void Install(Window& window);
    };
    struct FrameBufferQuery {
        void* context = nullptr;
        void (*get_size)(void*, void*, int*, int*) = nullptr;
    };
    class ScopedFrameBufferQuery {
        FrameBufferQuery _query;
        const FrameBufferQuery* _previous;

    public:
        explicit ScopedFrameBufferQuery(FrameBufferQuery query) noexcept;
        ScopedFrameBufferQuery(const ScopedFrameBufferQuery&) = delete;
        ScopedFrameBufferQuery& operator=(const ScopedFrameBufferQuery&) = delete;
        ~ScopedFrameBufferQuery();
    };
    d2::VectorI frame_buffer_size(void* native);
    std::shared_ptr<VSL_NAMESPACE::Surface> make_window_surface(VSL_NAMESPACE::Window& window, VSL_NAMESPACE::VulkanAccessor instance);
    std::shared_ptr<WindowData> make_window_data(const engine::EngineAccessor& engine,
                                                 std::shared_ptr<VSL_NAMESPACE::Surface> surface,
                                                 std::optional<VSL_NAMESPACE::PresentFencePolicy> present_fences = std::nullopt);
} // namespace AHO_NAMESPACE::window::detail
