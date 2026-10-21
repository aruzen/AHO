//
// Created by morimoto_hibiki on 2025/09/22.
//

#ifndef AHO_ALL_MOUSE_HPP
#define AHO_ALL_MOUSE_HPP

#include "input.hpp"
#include "input_manager.hpp"

namespace AHO_NAMESPACE::input {
    class Cursor : public abstract::Cursor, public abstract::Updatable {
        template<typename T, typename... Args>
        friend std::shared_ptr<T> AHO_NAMESPACE::InputManager::get(Args&&... args);

        void* _context = nullptr;
        d2::PointD _state;
        d2::VectorD _delta;

        explicit Cursor(void* context);

    public:
        d2::PointD state() override;

        d2::VectorD delta() override;

        void update() override;
    };

    class MouseButton : public abstract::Button, public abstract::Updatable {
        friend AHO_NAMESPACE::InputManager;

        void* _context = nullptr;
        MouseID _code;
        ButtonState _state = ButtonState::IDLE;

        MouseButton(void* context, MouseCode code);

        MouseButton(void* context, MouseID code);

    public:
        ButtonState state() override;

        bool down() override;

        bool up() override;

        bool pressed() override;

        void update() override;
    };

    class MouseWheel : public abstract::Input<d2::VectorD>, public abstract::Updatable {
        friend AHO_NAMESPACE::InputManager;

        void* _context = nullptr;
        size_t _gen, _read;

        static size_t SUBSCRIBED;

    public:
        static size_t GENERATION;
        static d2::VectorD STATE;

    private:
        MouseWheel(void* context);

    public:
        MouseWheel(const MouseWheel& other) noexcept;

        ~MouseWheel();

        d2::VectorD state() override;

        void update() override;
    };

    class Mouse : public abstract::Adaptable {
        friend AHO_NAMESPACE::InputManager;

        Mouse(void*, InputManager& manager);

    private:
        std::shared_ptr<Cursor> _cursor;
        std::shared_ptr<MouseButton> _right_click;
        std::shared_ptr<MouseButton> _middle_button;
        std::shared_ptr<MouseButton> _left_click;
        std::shared_ptr<MouseWheel> _wheel;

    public:
        std::shared_ptr<Cursor> cursor();

        std::shared_ptr<MouseButton> right_click();

        std::shared_ptr<MouseButton> middle_button();

        std::shared_ptr<MouseButton> left_click();

        std::shared_ptr<MouseWheel> wheel();
    };
} // namespace AHO_NAMESPACE::input

#endif //AHO_ALL_MOUSE_HPP
