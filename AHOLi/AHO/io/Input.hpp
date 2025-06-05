//
// Created by morimoto_hibiki on 2025/06/03.
//

#ifndef AHO_ALL_INPUT_HPP
#define AHO_ALL_INPUT_HPP

#include "../define.h"
#include "../const_values.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <array>


namespace AHO_NAMESPACE::input {
    enum class KeyCode {
        SPACE = 32,
        APOSTROPHE = 39,
        COMMA = 44,
        MINUS = 45,
        PERIOD = 46,
        SLASH = 47,
        NUM_0 = 48,
        NUM_1 = 49,
        NUM_2 = 50,
        NUM_3 = 51,
        NUM_4 = 52,
        NUM_5 = 53,
        NUM_6 = 54,
        NUM_7 = 55,
        NUM_8 = 56,
        NUM_9 = 57,
        SEMICOLON = 59,
        EQUAL = 61,
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        LEFT_BRACKET = 91,
        BACKSLASH = 92,
        RIGHT_BRACKET = 93,
        GRAVE_ACCENT = 96,
        WORLD_1 = 161,
        WORLD_2 = 162,
        ESCAPE = 256,
        ENTER = 257,
        TAB = 258,
        BACKSPACE = 259,
        INSERT = 260,
        DELETE = 261,
        RIGHT = 262,
        LEFT = 263,
        DOWN = 264,
        UP = 265,
        PAGE_UP = 266,
        PAGE_DOWN = 267,
        HOME = 268,
        END = 269,
        CAPS_LOCK = 280,
        SCROLL_LOCK = 281,
        NUM_LOCK = 282,
        PRINT_SCREEN = 283,
        PAUSE = 284,
        F1 = 290,
        F2 = 291,
        F3 = 292,
        F4 = 293,
        F5 = 294,
        F6 = 295,
        F7 = 296,
        F8 = 297,
        F9 = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,
        F13 = 302,
        F14 = 303,
        F15 = 304,
        F16 = 305,
        F17 = 306,
        F18 = 307,
        F19 = 308,
        F20 = 309,
        F21 = 310,
        F22 = 311,
        F23 = 312,
        F24 = 313,
        F25 = 314,
        KP_0 = 320,
        KP_1 = 321,
        KP_2 = 322,
        KP_3 = 323,
        KP_4 = 324,
        KP_5 = 325,
        KP_6 = 326,
        KP_7 = 327,
        KP_8 = 328,
        KP_9 = 329,
        KP_DECIMAL = 330,
        KP_DIVIDE = 331,
        KP_MULTIPLY = 332,
        KP_SUBTRACT = 333,
        KP_ADD = 334,
        KP_ENTER = 335,
        KP_EQUAL = 336,
        LEFT_SHIFT = 340,
        LEFT_CONTROL = 341,
        LEFT_ALT = 342,
        LEFT_SUPER = 343,
        RIGHT_SHIFT = 344,
        RIGHT_CONTROL = 345,
        RIGHT_ALT = 346,
        RIGHT_SUPER = 347,
        MENU = 348,
    };

    class KeyID {
        KeyCode _code;
    public:
        constexpr explicit KeyID(int code) : _code((KeyCode) code) {}

        constexpr explicit KeyID(KeyCode code) : _code(code) {}

        [[nodiscard]] constexpr KeyCode code() const {
            return _code;
        }

        [[nodiscard]] constexpr std::string name() const;
    };

    constexpr std::array<KeyID, 120> KeyCodes = {
            KeyID(KeyCode::SPACE),
            KeyID(KeyCode::APOSTROPHE),
            KeyID(KeyCode::COMMA),
            KeyID(KeyCode::MINUS),
            KeyID(KeyCode::PERIOD),
            KeyID(KeyCode::SLASH),
            KeyID(KeyCode::NUM_0),
            KeyID(KeyCode::NUM_1),
            KeyID(KeyCode::NUM_2),
            KeyID(KeyCode::NUM_3),
            KeyID(KeyCode::NUM_4),
            KeyID(KeyCode::NUM_5),
            KeyID(KeyCode::NUM_6),
            KeyID(KeyCode::NUM_7),
            KeyID(KeyCode::NUM_8),
            KeyID(KeyCode::NUM_9),
            KeyID(KeyCode::SEMICOLON),
            KeyID(KeyCode::EQUAL),
            KeyID(KeyCode::A),
            KeyID(KeyCode::B),
            KeyID(KeyCode::C),
            KeyID(KeyCode::D),
            KeyID(KeyCode::E),
            KeyID(KeyCode::F),
            KeyID(KeyCode::G),
            KeyID(KeyCode::H),
            KeyID(KeyCode::I),
            KeyID(KeyCode::J),
            KeyID(KeyCode::K),
            KeyID(KeyCode::L),
            KeyID(KeyCode::M),
            KeyID(KeyCode::N),
            KeyID(KeyCode::O),
            KeyID(KeyCode::P),
            KeyID(KeyCode::Q),
            KeyID(KeyCode::R),
            KeyID(KeyCode::S),
            KeyID(KeyCode::T),
            KeyID(KeyCode::U),
            KeyID(KeyCode::V),
            KeyID(KeyCode::W),
            KeyID(KeyCode::X),
            KeyID(KeyCode::Y),
            KeyID(KeyCode::Z),
            KeyID(KeyCode::LEFT_BRACKET),
            KeyID(KeyCode::BACKSLASH),
            KeyID(KeyCode::RIGHT_BRACKET),
            KeyID(KeyCode::GRAVE_ACCENT),
            KeyID(KeyCode::WORLD_1),
            KeyID(KeyCode::WORLD_2),
            KeyID(KeyCode::ESCAPE),
            KeyID(KeyCode::ENTER),
            KeyID(KeyCode::TAB),
            KeyID(KeyCode::BACKSPACE),
            KeyID(KeyCode::INSERT),
            KeyID(KeyCode::DELETE),
            KeyID(KeyCode::RIGHT),
            KeyID(KeyCode::LEFT),
            KeyID(KeyCode::DOWN),
            KeyID(KeyCode::UP),
            KeyID(KeyCode::PAGE_UP),
            KeyID(KeyCode::PAGE_DOWN),
            KeyID(KeyCode::HOME),
            KeyID(KeyCode::END),
            KeyID(KeyCode::CAPS_LOCK),
            KeyID(KeyCode::SCROLL_LOCK),
            KeyID(KeyCode::NUM_LOCK),
            KeyID(KeyCode::PRINT_SCREEN),
            KeyID(KeyCode::PAUSE),
            KeyID(KeyCode::F1),
            KeyID(KeyCode::F2),
            KeyID(KeyCode::F3),
            KeyID(KeyCode::F4),
            KeyID(KeyCode::F5),
            KeyID(KeyCode::F6),
            KeyID(KeyCode::F7),
            KeyID(KeyCode::F8),
            KeyID(KeyCode::F9),
            KeyID(KeyCode::F10),
            KeyID(KeyCode::F11),
            KeyID(KeyCode::F12),
            KeyID(KeyCode::F13),
            KeyID(KeyCode::F14),
            KeyID(KeyCode::F15),
            KeyID(KeyCode::F16),
            KeyID(KeyCode::F17),
            KeyID(KeyCode::F18),
            KeyID(KeyCode::F19),
            KeyID(KeyCode::F20),
            KeyID(KeyCode::F21),
            KeyID(KeyCode::F22),
            KeyID(KeyCode::F23),
            KeyID(KeyCode::F24),
            KeyID(KeyCode::F25),
            KeyID(KeyCode::KP_0),
            KeyID(KeyCode::KP_1),
            KeyID(KeyCode::KP_2),
            KeyID(KeyCode::KP_3),
            KeyID(KeyCode::KP_4),
            KeyID(KeyCode::KP_5),
            KeyID(KeyCode::KP_6),
            KeyID(KeyCode::KP_7),
            KeyID(KeyCode::KP_8),
            KeyID(KeyCode::KP_9),
            KeyID(KeyCode::KP_DECIMAL),
            KeyID(KeyCode::KP_DIVIDE),
            KeyID(KeyCode::KP_MULTIPLY),
            KeyID(KeyCode::KP_SUBTRACT),
            KeyID(KeyCode::KP_ADD),
            KeyID(KeyCode::KP_ENTER),
            KeyID(KeyCode::KP_EQUAL),
            KeyID(KeyCode::LEFT_SHIFT),
            KeyID(KeyCode::LEFT_CONTROL),
            KeyID(KeyCode::LEFT_ALT),
            KeyID(KeyCode::LEFT_SUPER),
            KeyID(KeyCode::RIGHT_SHIFT),
            KeyID(KeyCode::RIGHT_CONTROL),
            KeyID(KeyCode::RIGHT_ALT),
            KeyID(KeyCode::RIGHT_SUPER),
            KeyID(KeyCode::MENU)
    };

    enum class ButtonState {
        Idle, Pressed, Down, Up
    };

    namespace abstract {
        template<typename T>
        struct Input {
            virtual T state() = 0;
        };

        struct Updatable {
            virtual void update() {};
        };

        struct Button : public Input<ButtonState> {
            virtual bool down() = 0;

            virtual bool up() = 0;

            virtual bool pressed() = 0;
        };

        struct Text : public Input<std::string> {
        };

        struct Characters : public Input<std::unordered_set<KeyCode>> {
        };
    }
}


#endif //AHO_ALL_INPUT_HPP
