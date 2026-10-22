//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_WINDOW_H
#define AHO_ALL_WINDOW_H

#include "define.hpp"

#include <VSL/window.hpp>

#include "core/vector.hpp"
#include "engine.hpp"

namespace AHO_NAMESPACE::window {
    d2::VectorI GetMonitorSize();

    struct WindowData {
        std::shared_ptr<VSL_NAMESPACE::Surface> surface;
        VSL_NAMESPACE::SwapchainAccessor swapchain;
        VSL_NAMESPACE::RenderPass render_pass;
        VSL_NAMESPACE::FrameBuffer frame_buffer;
        VSL_NAMESPACE::SemaphoreHolder image_available, render_finished;
        VSL_NAMESPACE::FenceHolder in_flight;

        ~WindowData();
    };

    struct Window : public VSL_NAMESPACE::Window {
        Window(std::string name, int width = 800, int height = 600,
               engine::EngineAccessor engine = *engine::EngineAccessor::instance());

        Window(engine::EngineAccessor engine,
               std::shared_ptr<VSL_NAMESPACE::Window::WindowData> vsl_data,
               std::shared_ptr<AHO_NAMESPACE::window::WindowData> aho_data);

        engine::EngineAccessor engine;
        std::shared_ptr<AHO_NAMESPACE::window::WindowData> _data2;

        bool resize(d2::VectorI size);

        bool resize(int width, int height) override;

        // Recreates at the current extent, preserves usage and notifies resize plugins after GPU idle.
        // Also used for usage changes that do not resize the native window. Call outside recording.
        bool recreateSwapchain(VSL_NAMESPACE::ImageType additionalUsage = VSL_NAMESPACE::ImageType::None);

        d2::VectorI window_size();

        d2::VectorI frame_size();

        bool set_title(const std::string& title);

        void maximize();

        void restore();
        void show();
        void focus();

        bool close() override;
    };

    struct WindowResizeHookPlugin : public VSL_NAMESPACE::PureWindow::Plugin {
        // Called after resize or usage-driven swapchain recreation; the GPU is already idle.
        std::function<void(AHO_NAMESPACE::window::Window*)> hook;

        WindowResizeHookPlugin(VSL_NAMESPACE::PureWindow*, std::function<void(AHO_NAMESPACE::window::Window*)> hook);
    };
} // namespace AHO_NAMESPACE::window

namespace AHO_NAMESPACE {
    using window::Window;
}

inline bool AHO_NAMESPACE::window::Window::resize(AHO_NAMESPACE::d2::VectorI size) {
    return resize(size.value.x.value, size.value.y.value);
}

#endif //AHO_ALL_WINDOW_H
