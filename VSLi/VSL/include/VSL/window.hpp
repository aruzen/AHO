#pragma once
#include "define.hpp"
#include "exceptions.hpp"
#include "lifetime/window_owner.hpp"

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace VSL_NAMESPACE {
    namespace lifetime::detail {
        struct WindowAccess;
    }
    class WeakWindow;
    class WindowIdentity;

    class PureWindow {
    public:
        using PluginId = unsigned char;

        struct Plugin {
        private:
            const std::optional<PluginId> _requested_id;

        public:
            explicit Plugin(std::optional<PluginId> requested_id = std::nullopt) noexcept : _requested_id(requested_id) {}

            virtual ~Plugin() = default;

            std::optional<PluginId> requested_id() const noexcept { return _requested_id; }
        };
        struct WindowData;

        struct OnUpdate {
            virtual ~OnUpdate() = default;
            virtual bool on_update(PureWindow) = 0;
        };

        struct PluginSnapshot {
            PluginId id;
            std::weak_ptr<Plugin> plugin;
        };

    protected:
        class Call;

    private:
        std::shared_ptr<WindowData> _state;
        mutable Call* _calls = nullptr;
        explicit PureWindow(std::shared_ptr<WindowData> state) noexcept;
        void invalidate_calls() noexcept;
        static void RequirePluginTarget(const std::shared_ptr<WindowData>& state);
        static void PublishPlugin(const std::shared_ptr<WindowData>& state, std::shared_ptr<Plugin> plugin);
        friend struct lifetime::detail::WindowAccess;
        friend class WeakWindow;
        friend class WindowIdentity;

    public:
        PureWindow() noexcept = default;
        PureWindow(std::string name, int width = 800, int height = 600);
        PureWindow(const PureWindow& other) noexcept;
        PureWindow(PureWindow&& other) noexcept;
        PureWindow& operator=(PureWindow other) noexcept;
        virtual ~PureWindow();
        void swap(PureWindow& other) noexcept;

        friend void swap(PureWindow& left, PureWindow& right) noexcept { left.swap(right); }

        static bool Update();
        static bool NoCheckUpdate();
        static std::vector<PureWindow> Snapshot();

        template<typename T, typename... Args>
        std::shared_ptr<T> add_plugin(Args&&... args);
        bool remove_plugin(std::shared_ptr<Plugin> plugin);
        std::shared_ptr<Plugin> plugin(PluginId id) const;

        template<typename T>
        std::shared_ptr<T> plugin_as(PluginId id) const { return std::dynamic_pointer_cast<T>(plugin(id)); }

        std::optional<PluginId> plugin_id(const std::shared_ptr<Plugin>& plugin) const noexcept;
        std::vector<PluginSnapshot> plugin_snapshot() const;
        bool contains_plugin(PluginId id, const std::shared_ptr<Plugin>& plugin) const noexcept;

        virtual bool resize(int width, int height);
        std::string name() const;
        virtual bool close();
        lifetime::OSWindowOwner window_owner() const;
        bool should_close() const;
        void set_should_close(bool value);
        WeakWindow weak() const noexcept;
        WindowIdentity identity() const noexcept;
        explicit operator bool() const noexcept;
        bool operator==(const PureWindow& other) const noexcept;
    };

    // facadeの破棄・差し替えを検出する。状態の所有は呼出側の値snapshotが担う。
    class PureWindow::Call {
        const PureWindow* _target;
        Call* _previous;
        friend class PureWindow;

    public:
        explicit Call(const PureWindow& target) noexcept;
        Call(const Call&) = delete;
        Call& operator=(const Call&) = delete;
        ~Call();

        bool active() const noexcept { return _target != nullptr; }
    };

    class WindowIdentity {
        const PureWindow::WindowData* _pointer = nullptr;
        std::weak_ptr<PureWindow::WindowData> _owner;
        explicit WindowIdentity(const std::shared_ptr<PureWindow::WindowData>& state) noexcept;
        friend class PureWindow;

    public:
        WindowIdentity() noexcept = default;

        explicit operator bool() const noexcept { return _pointer != nullptr; }

        bool operator==(const WindowIdentity& other) const noexcept;
    };

    // OSWindowOwnerがNativeを保持していても、logical close後はlockできない。
    class WeakWindow {
        std::weak_ptr<PureWindow::WindowData> _state;

        explicit WeakWindow(const std::shared_ptr<PureWindow::WindowData>& state) noexcept : _state(state) {}
        friend class PureWindow;

    public:
        WeakWindow() noexcept = default;
        PureWindow lock() const noexcept;

        bool expired() const noexcept { return !lock(); }
    };

    class Window : public PureWindow {
    public:
        Window(std::string name, int width = 800, int height = 600);
        explicit Window(PureWindow source);
    };

    template<typename T, typename... Args>
    std::shared_ptr<T> PureWindow::add_plugin(Args&&... args) {
        static_assert(std::derived_from<T, Plugin>);
        Call call(*this);
        const auto state = _state;
        RequirePluginTarget(state);
        auto result = std::shared_ptr<T>(new T(this, std::forward<Args>(args)...));
        if (!call.active())
            throw exceptions::RuntimeException("window", "Window receiver changed during plugin construction");
        PublishPlugin(state, result);
        return result;
    }
} // namespace VSL_NAMESPACE
