#pragma once
#include "define.hpp"

#include "concepts.hpp"
#include "exceptions.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <string>

namespace VSL_NAMESPACE {
    namespace window_plugin {
        struct QuietClose;
    }

    class PureWindow {
    public:
        struct Plugin {
            unsigned char id = 0;
            virtual ~Plugin() {};
        };

        struct WindowData;
        struct OnUpdate {
            virtual ~OnUpdate() = default;
            virtual bool on_update(VSL_NAMESPACE::PureWindow) = 0;
        };

        struct WindowData {
            unsigned char plugins_next_id = 0;
            std::map<unsigned char, std::shared_ptr<Plugin>> plugins;
            std::vector<std::shared_ptr<OnUpdate>> update;

            // void* plugdata = nullptr;
            void* window_handle = nullptr;
            std::string name;
            int width, height;

            void destroy();
        };

        static std::vector<std::shared_ptr<WindowData>> WINDOWS;

    private:
        static bool INITED;

    public:
        PureWindow(std::string name, int width = 800, int height = 600);
        PureWindow(std::shared_ptr<WindowData> data_);
        PureWindow(const PureWindow&) = default;
        PureWindow& operator=(const PureWindow&) = default;
        PureWindow(PureWindow&&) = default;
        PureWindow& operator=(PureWindow&&) = default;
        virtual ~PureWindow() = default;

        static bool Update();
        static bool NoCheckUpdate();

        template<typename T, typename... Args>
        //	requires vsl::concepts::initializer<T, VSL_NAMESPACE::PureWindow::WindowData*, Args&&...>&& std::derived_from<T, VSL_NAMESPACE::PureWindow::Plugin>
        std::shared_ptr<T> add_plugin(Args&&... args);

        template<typename T, typename... Args>
        bool remove_plugin(std::shared_ptr<T> t);

        virtual bool resize(int width, int height);

        std::string name();
        virtual bool close();

        operator bool();
        bool operator==(const PureWindow& o);
        bool operator==(const PureWindow::WindowData* o);

        std::shared_ptr<WindowData> data_;
    };

    class Window : public PureWindow {
    public:
        Window(std::string name, int width = 800, int height = 600);
        Window(std::shared_ptr<WindowData> data);
    };

    template<typename T, typename... Args>
    // requires vsl::concepts::initializer<T, VSL_NAMESPACE::PureWindow::WindowData*, Args&&...>&& std::derived_from<T, VSL_NAMESPACE::PureWindow::Plugin>
    inline std::shared_ptr<T> VSL_NAMESPACE::PureWindow::add_plugin(Args&&... args) {
        auto t = std::shared_ptr<T>(new T(this, std::forward<Args>(args)...));
        std::shared_ptr<Plugin> p = std::static_pointer_cast<Plugin>(t);

        if (t->id == 0) {
            auto candidate = data_->plugins_next_id;
            while (data_->plugins.contains(candidate)) {
                candidate = static_cast<unsigned char>(candidate + 1);
                if (candidate == data_->plugins_next_id)
                    throw exceptions::RuntimeException("window", "No free window plugin IDs available");
            }
            p->id = candidate;
            data_->plugins_next_id = static_cast<unsigned char>(candidate + 1);
        }

        if (const auto previous = data_->plugins.find(p->id); previous != data_->plugins.end())
            if (auto listener = std::dynamic_pointer_cast<OnUpdate>(previous->second))
                std::erase(data_->update, listener);

        data_->plugins[t->id] = p;
        if (auto listener = std::dynamic_pointer_cast<OnUpdate>(p))
            data_->update.push_back(listener);

        return t;
    }

    template<typename T, typename... Args>
    inline bool VSL_NAMESPACE::PureWindow::remove_plugin(std::shared_ptr<T> t) {
        std::shared_ptr<Plugin> p = std::static_pointer_cast<Plugin>(t);
        if (!p)
            return false;
        const auto entry = this->data_->plugins.find(p->id);
        if (entry == this->data_->plugins.end() || entry->second != p)
            return false;
        if (auto listener = std::dynamic_pointer_cast<OnUpdate>(p))
            std::erase(this->data_->update, listener);
        this->data_->plugins.erase(entry);
        return true;
    }
} // namespace VSL_NAMESPACE
