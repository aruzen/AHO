#pragma once
#include <AHO/window.hpp>

namespace AHO_NAMESPACE::window::detail {
    std::shared_ptr<VSL_NAMESPACE::Surface> make_window_surface(VSL_NAMESPACE::Window& window, VSL_NAMESPACE::VulkanAccessor instance);
    std::shared_ptr<WindowData> make_window_data(const engine::EngineAccessor& engine,
                                                 std::shared_ptr<VSL_NAMESPACE::Surface> surface,
                                                 const std::string& name);
} // namespace AHO_NAMESPACE::window::detail
