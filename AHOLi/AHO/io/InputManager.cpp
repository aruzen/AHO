//
// Created by morimoto_hibiki on 2025/06/03.
//

#include "InputManager.hpp"

aho::InputManager::InputManager(void *context) : context(context){}

aho::InputManager::InputManager(vsl::PureWindow window) : context(window._data->window_handle) {}

void aho::InputManager::update() {
    for (auto itr = update_targets.begin(); itr != update_targets.end(); itr++){
        if (itr->expired()) {
            itr = update_targets.erase(itr);
            continue;
        }
        itr->lock()->update();
    }
}

aho::InputManager::operator bool() {
    update();
    return true;
}
