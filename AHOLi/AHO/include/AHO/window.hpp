//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_WINDOW_H
#define AHO_ALL_WINDOW_H

#include "define.hpp"

#include <VSL/window.hpp>

#include "core/vector.hpp"
#include "engine.hpp"
#include "render_context.hpp"

namespace AHO_NAMESPACE::window {
    d2::VectorI get_monitor_size();

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
               engine::EngineAccessor engine_ = DefaultEngine());

        Window(engine::EngineAccessor engine,
               std::shared_ptr<VSL_NAMESPACE::Window::WindowData> vsl_data,
               std::shared_ptr<AHO_NAMESPACE::window::WindowData> aho_data);

        engine::EngineAccessor engine;
        std::shared_ptr<AHO_NAMESPACE::window::WindowData> data2;

        RenderContext render_context() const;

        bool resize(d2::VectorI size);

        bool resize(int width, int height) override;

        // GPU完了後に標準描画資源を更新し、resize pluginへ通知する。
        // DrawPhaseの記録中と、同じwindowの再作成通知中には呼び出せない。
        bool recreate_swapchain(VSL_NAMESPACE::ImageType additional_usage = VSL_NAMESPACE::ImageType::NONE);

        d2::VectorI window_size();

        d2::VectorI frame_size();

        bool set_title(const std::string& title);

        void maximize();

        void restore();

        bool close() override;

    private:
        static AHO_NAMESPACE::engine::EngineAccessor DefaultEngine();
        void check_recreation_allowed();
    };

    struct WindowResizeHookPlugin : public VSL_NAMESPACE::PureWindow::Plugin {
        // native resizeとusage変更の両方で、GPU完了・標準描画資源の更新後に呼ばれる。
        // 同じwindowの再作成やGPU処理の送信は行わない。
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
