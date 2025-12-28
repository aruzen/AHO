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
        VSL_NAMESPACE::View<VSL_NAMESPACE::D2> image_view;
        VSL_NAMESPACE::RenderPass render_pass;
        VSL_NAMESPACE::FrameBuffer<VSL_NAMESPACE::D2> frame_buffer;
        VSL_NAMESPACE::SemaphoreHolder image_available, render_finished;
        VSL_NAMESPACE::FenceHolder in_flight;
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

        d2::VectorI window_size();

        d2::VectorI frame_size();

        bool set_title(const std::string& title);

        void maximize();

        void restore();
    };

    struct WindowResizeHookPlugin : public vsl::PureWindow::Plugin {
        std::function<void(aho::window::Window*)> hook;

        WindowResizeHookPlugin(vsl::PureWindow*, std::function<void(aho::window::Window*)> hook);
    };
}

namespace AHO_NAMESPACE {
    using window::Window;
}

inline bool aho::window::Window::resize(aho::d2::VectorI size) {
    return resize(size.value.x.value, size.value.y.value);
}

#endif //AHO_ALL_WINDOW_H
