//
// Created by morimoto_hibiki on 2025/06/03.
//

#ifndef AHO_ALL_KEY_HPP
#define AHO_ALL_KEY_HPP

#include "Input.hpp"
#include "InputManager.hpp"

namespace AHO_NAMESPACE::input {
    class Key : public abstract::Button, public abstract::Updatable {
        template<typename T, typename... Args>
        friend std::shared_ptr<T> InputManager::get(Args &&...args);

        void *context = nullptr;
        KeyID _code;
        ButtonState _state = ButtonState::Idle;

        Key(void *context, KeyCode code);

        Key(void *context, KeyID code);

    public:
        KeyID code();

        ButtonState state() override;

        void update() override;

        bool down() override;

        bool up() override;

        bool pressed() override;
    };

    template<typename ...KeyCodes>
    class Keys : public abstract::Adaptable {
        template<template<typename ...> typename T, typename... Args>
        friend std::shared_ptr<T<Args...>> InputManager::get(Args &&...args);

        Keys(void *context, aho::InputManager &manager, KeyCodes... codes);

    public:
        std::array<std::shared_ptr<Key>, sizeof...(KeyCodes)> keys;
    };

    template<typename... KeyCodes>
    Keys<KeyCodes...>::Keys(void *context, InputManager &manager, KeyCodes... codes) {
        std::tuple<KeyCodes...> packed_codes(codes...);
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((keys[Is] = manager.get<Key>(std::get<Is>(packed_codes))), ...);
        }(std::make_index_sequence<sizeof...(KeyCodes)>());
    }
}

#endif //AHO_ALL_KEY_HPP
