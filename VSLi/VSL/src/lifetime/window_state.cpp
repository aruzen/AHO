#include <VSL/lifetime/window_access.hpp>
#include <VSL/lifetime/window_runtime.hpp>
#include <VSL/operation/window_ops.hpp>
#include <algorithm>
#include <stdexcept>

namespace VSL_NAMESPACE {
    namespace {
        bool same_plugin(const std::shared_ptr<PureWindow::Plugin>& left, const std::shared_ptr<PureWindow::Plugin>& right) noexcept {
            return left.get() == right.get() && !left.owner_before(right) && !right.owner_before(left);
        }
    } // namespace

    std::vector<std::shared_ptr<PureWindow::WindowData>>& lifetime::detail::WindowAccess::Registry() noexcept {
        static std::vector<std::shared_ptr<PureWindow::WindowData>> WINDOWS;
        return WINDOWS;
    }

    PureWindow lifetime::detail::WindowAccess::FindNative(void* native) noexcept {
        for (const auto& state : Registry())
            if (state->window_handle == native && !state->closed)
                return FromState(state);
        return {};
    }

    void PureWindow::RequirePluginTarget(const std::shared_ptr<WindowData>& state) {
        if (!state)
            throw exceptions::RuntimeException("window", "Window data is required");
        if (state->closed)
            throw std::invalid_argument("Cannot register a plugin on a closed window");
    }

    void PureWindow::PublishPlugin(const std::shared_ptr<WindowData>& state, std::shared_ptr<Plugin> plugin) {
        RequirePluginTarget(state);
        auto candidate = plugin->requested_id().value_or(state->plugins_next_id);
        if (!plugin->requested_id()) {
            while (state->plugins.contains(candidate)) {
                candidate = static_cast<PluginId>(candidate + 1);
                if (candidate == state->plugins_next_id)
                    throw exceptions::RuntimeException("window", "No free window plugin IDs available");
            }
        }
        const bool automatic = !plugin->requested_id();
        const bool listener = dynamic_cast<OnUpdate*>(plugin.get()) != nullptr;
        if (listener)
            state->update.reserve(state->update.size() + 1);
        std::shared_ptr<Plugin> retired;
        const auto previous = state->plugins.find(candidate);
        if (previous == state->plugins.end())
            state->plugins.emplace(candidate, std::move(plugin));
        else {
            retired = std::move(previous->second);
            previous->second = std::move(plugin);
        }
        std::erase(state->update, candidate);
        if (listener)
            state->update.push_back(candidate);
        if (automatic)
            state->plugins_next_id = static_cast<PluginId>(candidate + 1);
        // 旧pluginのdestructorは、登録表と購読順序のcommit後にのみ実行する。
    }

    bool PureWindow::remove_plugin(std::shared_ptr<Plugin> plugin) {
        const auto state = _state;
        if (!state || !plugin)
            return false;
        const auto entry = std::ranges::find_if(state->plugins, [&](const auto& item) { return same_plugin(item.second, plugin); });
        if (entry == state->plugins.end())
            return false;
        std::erase(state->update, entry->first);
        state->plugins.erase(entry);
        return true;
    }

    std::shared_ptr<PureWindow::Plugin> PureWindow::plugin(PluginId id) const {
        if (!_state)
            return {};
        const auto entry = _state->plugins.find(id);
        return entry == _state->plugins.end() ? std::shared_ptr<Plugin>{} : entry->second;
    }

    std::optional<PureWindow::PluginId> PureWindow::plugin_id(const std::shared_ptr<Plugin>& plugin) const noexcept {
        if (_state)
            for (const auto& [id, entry] : _state->plugins)
                if (same_plugin(entry, plugin))
                    return id;
        return std::nullopt;
    }

    bool PureWindow::contains_plugin(PluginId id, const std::shared_ptr<Plugin>& plugin) const noexcept {
        if (!_state || !plugin)
            return false;
        const auto entry = _state->plugins.find(id);
        return entry != _state->plugins.end() && same_plugin(entry->second, plugin);
    }

    std::vector<PureWindow::PluginSnapshot> PureWindow::plugin_snapshot() const {
        std::vector<PluginSnapshot> result;
        if (_state) {
            result.reserve(_state->plugins.size());
            for (const auto& [id, plugin] : _state->plugins)
                result.push_back({id, plugin});
        }
        return result;
    }

    std::vector<PureWindow> PureWindow::Snapshot() {
        std::vector<PureWindow> result;
        const auto& windows = lifetime::detail::WindowAccess::Registry();
        result.reserve(windows.size());
        for (const auto& state : windows)
            result.push_back(PureWindow(state));
        return result;
    }

    bool PureWindow::NoCheckUpdate() {
        lifetime::GLFWCallScope scope;
        if (lifetime::in_glfw_callback())
            throw std::logic_error("Cannot poll GLFW from a callback");
        if (lifetime::glfw_runtime_active())
            operation::poll_os_windows();
        return true;
    }

    bool PureWindow::Update() {
        lifetime::GLFWCallScope scope;
        NoCheckUpdate();
        const auto& registry = lifetime::detail::WindowAccess::Registry();
        const std::vector<std::weak_ptr<WindowData>> windows(registry.begin(), registry.end());
        bool result = true;
        for (const auto& candidate : windows) {
            const auto state = candidate.lock();
            if (!state || std::ranges::find(registry, state) == registry.end())
                continue;
            const PureWindow window(state);
            std::vector<PluginSnapshot> callbacks;
            callbacks.reserve(state->update.size());
            for (const auto id : state->update)
                callbacks.push_back({id, state->plugins.at(id)});
            for (const auto& callback : callbacks) {
                if (state->closed || std::ranges::find(registry, state) == registry.end())
                    break;
                const auto owner = callback.plugin.lock();
                if (!window.contains_plugin(callback.id, owner))
                    continue;
                if (auto* listener = dynamic_cast<OnUpdate*>(owner.get()); listener && !listener->on_update(window))
                    result = false;
            }
        }
        return result;
    }

    void PureWindow::WindowData::destroy() {
        if (!window_handle)
            return;
        auto& registry = lifetime::detail::WindowAccess::Registry();
        const auto registered = std::ranges::find_if(registry, [this](const auto& state) { return state.get() == this; });
        const auto keep = registered == registry.end() ? std::shared_ptr<WindowData>{} : *registered;
        lifetime::GLFWCallScope scope;
        if (!window_handle)
            return;
        auto owner = std::move(os_window);
        const auto native = std::exchange(window_handle, nullptr);
        closed = true;
        std::erase_if(registry, [this](const auto& state) { return state.get() == this; });
        decltype(plugins) retired_plugins;
        decltype(update) retired_update;
        plugins.swap(retired_plugins);
        update.swap(retired_update);
        operation::detach_os_window(native);
    }
} // namespace VSL_NAMESPACE
