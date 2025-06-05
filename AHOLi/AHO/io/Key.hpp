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
        friend std::shared_ptr<T> InputManager::get(Args && ...args);

        void* context = nullptr;
        KeyID code;
        ButtonState _state = ButtonState::Idle;

        Key(void * context, KeyCode code);
        Key(void * context, KeyID code);
    public:
        ButtonState state() override;

        void update() override;

        bool down() override;

        bool up() override;

        bool pressed() override;
    };
}


#endif //AHO_ALL_KEY_HPP
