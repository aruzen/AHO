//
// Created by morimoto_hibiki on 2025/06/03.
//

#include <AHO/io/input_manager.hpp>

aho::InputManager::InputManager(void *context) : context(context){}

aho::InputManager::InputManager(vsl::PureWindow window) : context(window._data->window_handle) {}

void aho::InputManager::update() {
    const auto targets = update_targets;
    for (const auto &entry : targets) {
        if (auto target = entry.lock()) {
            target->update();
        }
    }
    std::erase_if(update_targets, [](const auto &entry) { return entry.expired(); });
}

aho::InputManager::operator bool() {
    update();
    return true;
}
