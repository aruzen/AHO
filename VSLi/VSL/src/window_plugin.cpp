#include <VSL/window_plugin.hpp>
#include <VSL/utils/string.hpp>
#include <VSL/lifetime/window_access.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#include <VSL/operation/window_ops.hpp>
#include <stdexcept>

namespace {
    void framebuffer_resize_callback(GLFWwindow* native, int width, int height) {
        VSL_NAMESPACE::lifetime::GLFWCallbackScope scope;
        auto window = VSL_NAMESPACE::lifetime::detail::WindowAccess::FindNative(native);
        if (!window)
            return;
        const auto owner = window.window_owner();
        const auto plugin = window.plugin(VSL_NAMESPACE::window_plugin::HookResize::HOOK_RESIZE_USED_ID);
        if (auto* hook = dynamic_cast<VSL_NAMESPACE::window_plugin::HookResize*>(plugin.get())) {
            const auto callback = hook->callback;
            if (callback)
                callback(std::move(window), width, height);
        }
    }
} // namespace

bool VSL_NAMESPACE::window_plugin::QuietClose::on_update(PureWindow window) {
    if (window.should_close()) {
        logging("destroy " + window.name() + " by QuietClosePlugin");
        window.close();
    }
    return true;
}
VSL_NAMESPACE::window_plugin::QuietClose::QuietClose(PureWindow*) : Plugin(QUIET_CLOSE_USED_ID) {}

bool VSL_NAMESPACE::window_plugin::HookShouldClose::on_update(PureWindow window) {
    const auto action = callback;
    if (window.should_close() && action)
        action(std::move(window));
    return true;
}

VSL_NAMESPACE::window_plugin::HookResize::HookResize(PureWindow* window, std::function<void(PureWindow, int, int)> callback)
    : Plugin(HOOK_RESIZE_USED_ID), callback(std::move(callback)) {
    if (!window)
        throw std::invalid_argument("Resize hook requires a live window");
    const PureWindow snapshot = *window;
    const auto owner = snapshot.window_owner();
    lifetime::GLFWCallScope scope;
    if (!snapshot)
        throw std::invalid_argument("Resize hook requires a live window");
    operation::set_os_window_resize_callback(owner.native_handle(), framebuffer_resize_callback);
}
