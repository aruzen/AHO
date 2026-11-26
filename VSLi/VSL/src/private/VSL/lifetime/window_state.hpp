#pragma once
#include <VSL/window.hpp>
#include <map>

namespace VSL_NAMESPACE {
    struct PureWindow::WindowData {
        PluginId plugins_next_id = 0;
        std::map<PluginId, std::shared_ptr<Plugin>> plugins;
        // 登録表が唯一の所有者。更新順序には登録IDだけを保存する。
        std::vector<PluginId> update;
        void* window_handle = nullptr;
        lifetime::OSWindowOwner os_window;
        std::string name;
        int width = 0, height = 0;
        bool closed = false;
        void destroy();
    };
} // namespace VSL_NAMESPACE
