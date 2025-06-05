//
// Created by morimoto_hibiki on 2025/06/03.
//

#ifndef AHO_ALL_INPUTMANAGER_HPP
#define AHO_ALL_INPUTMANAGER_HPP

#include "Input.hpp"

#include <memory>
#include <vector>
#include <concepts>

#include <VSL/Window.h>

namespace AHO_NAMESPACE {
    struct InputManager {
        void* context = nullptr;
        std::vector<std::weak_ptr<input::abstract::Updatable>> update_targets;

        InputManager(void* context);

        InputManager(VSL_NAMESPACE::PureWindow window);

        void update();

        operator bool();

        template<typename T, typename ...Args>
        std::shared_ptr<T> get(Args&& ...args);
    };

    template<typename T, typename... Args>
    std::shared_ptr<T> InputManager::get(Args && ...args) {
        std::shared_ptr<T> input(new T(context, std::forward<Args>(args)...));

        if (auto u = std::dynamic_pointer_cast<input::abstract::Updatable>(input); u) {
            std::weak_ptr<input::abstract::Updatable> wptr = u;
            update_targets.push_back(wptr);
        }

        return input;
    }
}

#endif //AHO_ALL_INPUTMANAGER_HPP
