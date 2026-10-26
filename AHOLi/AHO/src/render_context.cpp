#include <AHO/render_context.hpp>
#include <AHO/window.hpp>
#include <stdexcept>

namespace AHO_NAMESPACE {
    RenderContext::RenderContext(const window::Window& window)
        : _engine(window.engine.data_), _target(window.data2), _window(window.data_) {
        require_live();
    }

    void RenderContext::require_live() const {
        if (!_window || !_window->window_handle || !_target || !_engine ||
            !_engine->logical_device.data_ || !_engine->command_manager.data_ || !_engine->graphic_resource_manager)
            throw std::logic_error("RenderContext requires a live, booted AHO window");
    }

    VSL_NAMESPACE::LogicalDeviceAccessor RenderContext::device() const {
        require_live();
        return _engine->logical_device;
    }

    VSL_NAMESPACE::graphic_resource::ManagerInterface& RenderContext::descriptor_manager() const {
        require_live();
        return *_engine->graphic_resource_manager;
    }

    VSL_NAMESPACE::CommandManager RenderContext::commands() const {
        require_live();
        return _engine->command_manager;
    }

    size_t RenderContext::frame_count() const {
        return commands().get_buffer().get_size();
    }

    VSL_NAMESPACE::RenderPass RenderContext::render_pass() const {
        require_live();
        return _target->render_pass;
    }

    VSL_NAMESPACE::data_format::Format RenderContext::format() const {
        require_live();
        return _target->swapchain.get_format();
    }

    VSL_NAMESPACE::Viewport RenderContext::viewport() const {
        require_live();
        return VSL_NAMESPACE::Viewport(_target->swapchain);
    }

    VSL_NAMESPACE::Scissor RenderContext::scissor() const {
        require_live();
        return VSL_NAMESPACE::Scissor(_target->swapchain);
    }

    RenderContext window::Window::render_context() const {
        return RenderContext(*this);
    }
} // namespace AHO_NAMESPACE
