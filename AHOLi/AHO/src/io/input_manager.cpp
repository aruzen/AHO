//
// Created by morimoto_hibiki on 2025/06/03.
//

#include <AHO/io/input_manager.hpp>

AHO_NAMESPACE::InputManager::InputManager(void* context) : context(context) {}

AHO_NAMESPACE::InputManager::InputManager(VSL_NAMESPACE::PureWindow window) : context(window.data_->window_handle) {}

void AHO_NAMESPACE::InputManager::update() {
    const auto targets = update_targets;
    for (const auto& entry : targets) {
        if (auto target = entry.lock()) {
            target->update();
        }
    }
    std::erase_if(update_targets, [](const auto& entry) { return entry.expired(); });
}

AHO_NAMESPACE::InputManager::operator bool() {
    update();
    return true;
}
