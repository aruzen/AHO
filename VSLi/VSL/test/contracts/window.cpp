#include <VSL/window.hpp>
#include <VSL/window_plugin.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

#if VSL_TYPED_CASE == 0
static_assert(std::is_nothrow_copy_constructible_v<vsl::PureWindow>);
static_assert(std::is_nothrow_move_constructible_v<vsl::PureWindow>);
static_assert(std::is_nothrow_move_assignable_v<vsl::PureWindow>);
static_assert(noexcept(vsl::WeakWindow{}.lock()));
static_assert(!std::is_aggregate_v<vsl::PureWindow>);
void use_window(vsl::PureWindow window);
void use_window(vsl::PureWindow window) {
    const auto identity = window.identity();
    const auto weak = window.weak();
    const auto copy = weak.lock();
    for (const auto& [id, candidate] : window.plugin_snapshot())
        (void)window.contains_plugin(id, candidate.lock());
    (void)(identity == copy.identity());
    (void)vsl::PureWindow::Snapshot();
    auto hook = window.add_plugin<vsl::window_plugin::HookShouldClose>([](vsl::PureWindow) {});
    (void)window.plugin_id(hook);
    window.remove_plugin(std::move(hook));
}
#elif VSL_TYPED_CASE == 1
void reject(vsl::PureWindow window) {
    (void)window.data_;
}
#elif VSL_TYPED_CASE == 2
void reject(vsl::PureWindow window) {
    (void)window._state;
}
#elif VSL_TYPED_CASE == 3
auto reject = sizeof(vsl::PureWindow::WindowData);
#elif VSL_TYPED_CASE == 4
auto reject = vsl::PureWindow::WINDOWS;
#elif VSL_TYPED_CASE == 5
vsl::PureWindow reject(std::shared_ptr<vsl::PureWindow::WindowData>{});
#elif VSL_TYPED_CASE == 6
void reject(vsl::PureWindow::Plugin& plugin) {
    plugin.id = 0;
}
#elif VSL_TYPED_CASE == 7
vsl::WeakWindow reject(std::shared_ptr<vsl::PureWindow::WindowData>{});
#elif VSL_TYPED_CASE == 8
void reject(vsl::WindowIdentity& identity) {
    (void)identity._pointer;
}
#elif VSL_TYPED_CASE == 9
vsl::window_plugin::HookShouldClose reject(static_cast<vsl::PureWindow::WindowData*>(nullptr), [](vsl::PureWindow) {});
#endif
