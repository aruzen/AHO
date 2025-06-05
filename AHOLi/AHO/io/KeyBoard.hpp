//
// Created by morimoto_hibiki on 2025/06/04.
//

#ifndef AHO_ALL_KEYBOARD_HPP
#define AHO_ALL_KEYBOARD_HPP

#include "Input.hpp"
#include "InputManager.hpp"

namespace AHO_NAMESPACE::input {
    class KeyBoard : abstract::Characters {
        template<typename T, typename... Args>
        friend std::shared_ptr<T> InputManager::get(Args && ...args);

        void* context = nullptr;

        KeyBoard(void * context);
    public:
        ~KeyBoard();

        std::unordered_set<KeyCode> state() override;

        bool pressed(KeyCode code) const;
    };
}

#endif //AHO_ALL_KEYBOARD_HPP
