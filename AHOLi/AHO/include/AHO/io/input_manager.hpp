//
// Created by morimoto_hibiki on 2025/06/03.
//

#ifndef AHO_ALL_INPUTMANAGER_HPP
#define AHO_ALL_INPUTMANAGER_HPP

#include "input.hpp"

#include <memory>
#include <vector>
#include <concepts>

#include "VSL/window.hpp"

namespace AHO_NAMESPACE {
    struct InputManager {
    private:
        VSL_NAMESPACE::lifetime::OSWindowOwner _window_owner;

    public:
        void* context = nullptr;
        std::vector<std::weak_ptr<input::abstract::Updatable>> update_targets;

        InputManager(void* context);

        InputManager(VSL_NAMESPACE::PureWindow window);

        void update();

        operator bool();

        template<typename T, typename... Args>
        std::shared_ptr<T> get(Args&&... args);

        template<template<typename...> typename T, typename... Args>
        std::shared_ptr<T<Args...>> get(Args&&... args);
    };

    template<typename T, typename... Args>
    std::shared_ptr<T> InputManager::get(Args&&... args) {
        const auto owner = _window_owner;
        std::shared_ptr<T> input;
        if constexpr (std::derived_from<T, input::abstract::Adaptable>) {
            input = std::shared_ptr<T>(new T(context, *this, std::forward<Args>(args)...));
        } else {
            input = std::shared_ptr<T>(new T(context, std::forward<Args>(args)...));
        }
        if constexpr (std::derived_from<T, input::abstract::WindowBound>)
            static_cast<input::abstract::WindowBound&>(*input)._window_owner = owner;
        if constexpr (std::derived_from<T, input::abstract::Updatable>) {
            update_targets.push_back(input);
        }
        return input;
    }

    template<template<typename...> typename T, typename... Args>
    std::shared_ptr<T<Args...>> InputManager::get(Args&&... args) {
        const auto owner = _window_owner;
        using Type = T<Args...>;
        std::shared_ptr<Type> input;
        if constexpr (std::derived_from<Type, input::abstract::Adaptable>) {
            input = std::shared_ptr<Type>(new Type(context, *this, std::forward<Args>(args)...));
        } else {
            input = std::shared_ptr<Type>(new Type(context, std::forward<Args>(args)...));
        }
        if constexpr (std::derived_from<Type, input::abstract::WindowBound>)
            static_cast<input::abstract::WindowBound&>(*input)._window_owner = owner;
        if constexpr (std::derived_from<Type, input::abstract::Updatable>) {
            update_targets.push_back(input);
        }

        return input;
    }
} // namespace AHO_NAMESPACE

#endif //AHO_ALL_INPUTMANAGER_HPP
