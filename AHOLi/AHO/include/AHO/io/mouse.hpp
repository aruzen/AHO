//
// Created by morimoto_hibiki on 2025/09/22.
//

#ifndef AHO_ALL_MOUSE_HPP
#define AHO_ALL_MOUSE_HPP

#include "input.hpp"
#include "input_manager.hpp"

namespace AHO_NAMESPACE::input {
    class Cursor : public abstract::Cursor, public abstract::Updatable {
        template<typename T, typename ...Args>
        friend std::shared_ptr<T> aho::InputManager::get(Args &&...args);

        void *context = nullptr;
        d2::PointD _state;
        d2::VectorD _delta;

        explicit Cursor(void *context);

    public:
        d2::PointD state() override;

        d2::VectorD delta() override;

        void update() override;
    };

    class MouseButton : public abstract::Button, public abstract::Updatable {
        friend aho::InputManager;

        void *context = nullptr;
        MouseID code;
        ButtonState _state = ButtonState::Idle;

        MouseButton(void *context, MouseCode code);

        MouseButton(void *context, MouseID code);

    public:
        ButtonState state() override;

        bool down() override;

        bool up() override;

        bool pressed() override;

        void update() override;
    };

    class MouseWheel : public abstract::Input<d2::VectorD>, public abstract::Updatable {
        friend aho::InputManager;

        void *context = nullptr;
        size_t gen, read;

        static size_t subscribed;
    public:
        static size_t _generation;
        static d2::VectorD _state;
    private:
        MouseWheel(void *context);
    public:
        ~MouseWheel();

        d2::VectorD state() override;

        void update() override;
    };

    class Mouse : public abstract::Adaptable {
        friend aho::InputManager;

        Mouse(void*, InputManager &manager);
    private:

        std::shared_ptr<Cursor> _cursor;
        std::shared_ptr<MouseButton> _rightClick;
        std::shared_ptr<MouseButton> _middleButton;
        std::shared_ptr<MouseButton> _leftClick;
        std::shared_ptr<MouseWheel> _wheel;
    public:
        std::shared_ptr<Cursor> cursor();

        std::shared_ptr<MouseButton> rightClick();

        std::shared_ptr<MouseButton> middleButton();

        std::shared_ptr<MouseButton> leftClick();

        std::shared_ptr<MouseWheel> wheel();
    };
}


#endif //AHO_ALL_MOUSE_HPP
