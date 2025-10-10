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

        template<template<typename ...> typename T, typename ...Args>
        std::shared_ptr<T<Args...>> get(Args&& ...args);
    };

    template<typename T, typename... Args>
    std::shared_ptr<T> InputManager::get(Args && ...args) {
        std::shared_ptr<T> input;
        if constexpr (std::derived_from<T, input::abstract::Adaptable>) {
            input = std::shared_ptr<T>(new T(context, *this, std::forward<Args>(args)...));
        } else {
            input = std::shared_ptr<T>(new T(context, std::forward<Args>(args)...));

            if (auto u = std::dynamic_pointer_cast<input::abstract::Updatable>(input); u) {
                std::weak_ptr<input::abstract::Updatable> wptr = u;
                update_targets.push_back(wptr);
            }
        }
        return input;
    }

    template<template<typename...> typename T, typename... Args>
    std::shared_ptr<T<Args...>> InputManager::get(Args && ...args) {
        using Type = T<Args...>;
        std::shared_ptr<Type> input;
        if constexpr (std::derived_from<Type, input::abstract::Adaptable>) {
            input = std::shared_ptr<Type>(new Type(context, *this, std::forward<Args>(args)...));
        } else {
            input = std::shared_ptr<Type>(new Type(context, std::forward<Args>(args)...));

            if (auto u = std::dynamic_pointer_cast<input::abstract::Updatable>(input); u) {
                std::weak_ptr<input::abstract::Updatable> wptr = u;
                update_targets.push_back(wptr);
            }
        }

        return input;
    }
}

#endif //AHO_ALL_INPUTMANAGER_HPP
