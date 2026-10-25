#pragma once
#include "engine.hpp"
#include <VSL/window.hpp>

namespace AHO_NAMESPACE::window {
    struct Window;
    struct WindowData;
} // namespace AHO_NAMESPACE::window

namespace AHO_NAMESPACE {
    // A live window's drawing services. Target accessors follow swapchain recreation.
    class RenderContext {
        std::shared_ptr<engine::EngineData> _engine;
        std::shared_ptr<window::WindowData> _target;
        std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData> _window;
        friend struct window::Window;
        RenderContext(const window::Window& window);
        void require_live() const;

    public:
        VSL_NAMESPACE::LogicalDeviceAccessor device() const;
        VSL_NAMESPACE::graphic_resource::ManagerInterface& descriptor_manager() const;
        VSL_NAMESPACE::CommandManager commands() const;
        size_t frame_count() const;
        VSL_NAMESPACE::RenderPass render_pass() const;
        VSL_NAMESPACE::data_format::Format format() const;
        VSL_NAMESPACE::Viewport viewport() const;
        VSL_NAMESPACE::Scissor scissor() const;
    };
} // namespace AHO_NAMESPACE
