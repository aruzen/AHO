//
// Created by morimoto_hibiki on 2025/06/03.
//

#ifndef AHO_ALL_INPUT_HPP
#define AHO_ALL_INPUT_HPP

#include "../define.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <array>

#include "../core/point.hpp"
#include "../core/vector.hpp"

namespace AHO_NAMESPACE {
    struct InputManager;
}
namespace AHO_NAMESPACE::input {
    enum class KeyCode {
        SPACE = 32,
        APOSTROPHE = 39,
        COMMA = 44,
        MINUS = 45,
        PERIOD = 46,
        SLASH = 47,
        NUM0 = 48,
        NUM1 = 49,
        NUM2 = 50,
        NUM3 = 51,
        NUM4 = 52,
        NUM5 = 53,
        NUM6 = 54,
        NUM7 = 55,
        NUM8 = 56,
        NUM9 = 57,
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
        WORLD1 = 161,
        WORLD2 = 162,
        ESCAPE = 256,
        ENTER = 257,
        TAB = 258,
        BACKSPACE = 259,
        INSERT = 260,
        DELETE_ = 261,
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
        KEY_PAD0 = 320,
        KEY_PAD1 = 321,
        KEY_PAD2 = 322,
        KEY_PAD3 = 323,
        KEY_PAD4 = 324,
        KEY_PAD5 = 325,
        KEY_PAD6 = 326,
        KEY_PAD7 = 327,
        KEY_PAD8 = 328,
        KEY_PAD9 = 329,
        KEY_PAD_DECIMAL = 330,
        KEY_PAD_DIVIDE = 331,
        KEY_PAD_MULTIPLY = 332,
        KEY_PAD_SUBTRACT = 333,
        KEY_PAD_ADD = 334,
        KEY_PAD_ENTER = 335,
        KEY_PAD_EQUAL = 336,
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

    enum class MouseCode {
        BUTTON1 = 0,
        BUTTON2 = 1,
        BUTTON3 = 2,
        BUTTON4 = 3,
        BUTTON5 = 4,
        BUTTON6 = 5,
        BUTTON7 = 6,
        BUTTON8 = 7,
        LAST_BUTTON = BUTTON8,
        LEFT_CLICK = BUTTON1,
        RIGHT_CLICK = BUTTON2,
        MIDDLE_BUTTON = BUTTON3,
    };

    constexpr std::array<KeyCode, 120> KEY_CODES = {
        KeyCode::SPACE,
        KeyCode::APOSTROPHE,
        KeyCode::COMMA,
        KeyCode::MINUS,
        KeyCode::PERIOD,
        KeyCode::SLASH,
        KeyCode::NUM0,
        KeyCode::NUM1,
        KeyCode::NUM2,
        KeyCode::NUM3,
        KeyCode::NUM4,
        KeyCode::NUM5,
        KeyCode::NUM6,
        KeyCode::NUM7,
        KeyCode::NUM8,
        KeyCode::NUM9,
        KeyCode::SEMICOLON,
        KeyCode::EQUAL,
        KeyCode::A,
        KeyCode::B,
        KeyCode::C,
        KeyCode::D,
        KeyCode::E,
        KeyCode::F,
        KeyCode::G,
        KeyCode::H,
        KeyCode::I,
        KeyCode::J,
        KeyCode::K,
        KeyCode::L,
        KeyCode::M,
        KeyCode::N,
        KeyCode::O,
        KeyCode::P,
        KeyCode::Q,
        KeyCode::R,
        KeyCode::S,
        KeyCode::T,
        KeyCode::U,
        KeyCode::V,
        KeyCode::W,
        KeyCode::X,
        KeyCode::Y,
        KeyCode::Z,
        KeyCode::LEFT_BRACKET,
        KeyCode::BACKSLASH,
        KeyCode::RIGHT_BRACKET,
        KeyCode::GRAVE_ACCENT,
        KeyCode::WORLD1,
        KeyCode::WORLD2,
        KeyCode::ESCAPE,
        KeyCode::ENTER,
        KeyCode::TAB,
        KeyCode::BACKSPACE,
        KeyCode::INSERT,
        KeyCode::DELETE_,
        KeyCode::RIGHT,
        KeyCode::LEFT,
        KeyCode::DOWN,
        KeyCode::UP,
        KeyCode::PAGE_UP,
        KeyCode::PAGE_DOWN,
        KeyCode::HOME,
        KeyCode::END,
        KeyCode::CAPS_LOCK,
        KeyCode::SCROLL_LOCK,
        KeyCode::NUM_LOCK,
        KeyCode::PRINT_SCREEN,
        KeyCode::PAUSE,
        KeyCode::F1,
        KeyCode::F2,
        KeyCode::F3,
        KeyCode::F4,
        KeyCode::F5,
        KeyCode::F6,
        KeyCode::F7,
        KeyCode::F8,
        KeyCode::F9,
        KeyCode::F10,
        KeyCode::F11,
        KeyCode::F12,
        KeyCode::F13,
        KeyCode::F14,
        KeyCode::F15,
        KeyCode::F16,
        KeyCode::F17,
        KeyCode::F18,
        KeyCode::F19,
        KeyCode::F20,
        KeyCode::F21,
        KeyCode::F22,
        KeyCode::F23,
        KeyCode::F24,
        KeyCode::F25,
        KeyCode::KEY_PAD0,
        KeyCode::KEY_PAD1,
        KeyCode::KEY_PAD2,
        KeyCode::KEY_PAD3,
        KeyCode::KEY_PAD4,
        KeyCode::KEY_PAD5,
        KeyCode::KEY_PAD6,
        KeyCode::KEY_PAD7,
        KeyCode::KEY_PAD8,
        KeyCode::KEY_PAD9,
        KeyCode::KEY_PAD_DECIMAL,
        KeyCode::KEY_PAD_DIVIDE,
        KeyCode::KEY_PAD_MULTIPLY,
        KeyCode::KEY_PAD_SUBTRACT,
        KeyCode::KEY_PAD_ADD,
        KeyCode::KEY_PAD_ENTER,
        KeyCode::KEY_PAD_EQUAL,
        KeyCode::LEFT_SHIFT,
        KeyCode::LEFT_CONTROL,
        KeyCode::LEFT_ALT,
        KeyCode::LEFT_SUPER,
        KeyCode::RIGHT_SHIFT,
        KeyCode::RIGHT_CONTROL,
        KeyCode::RIGHT_ALT,
        KeyCode::RIGHT_SUPER,
        KeyCode::MENU,
    };

    constexpr std::array<MouseCode, 8> MOUSE_CODES{
        MouseCode::LEFT_CLICK,
        MouseCode::RIGHT_CLICK,
        MouseCode::MIDDLE_BUTTON,
        MouseCode::BUTTON4,
        MouseCode::BUTTON5,
        MouseCode::BUTTON6,
        MouseCode::BUTTON7,
        MouseCode::LAST_BUTTON,
    };

    enum class ButtonState {
        IDLE,
        PRESSED,
        DOWN,
        UP
    };

    template<typename CodeType>
    class CodeID {
        CodeType _code;

    public:
        constexpr explicit CodeID(int code) : _code(static_cast<CodeType>(code)) {}

        constexpr explicit CodeID(CodeType code) : _code(code) {}

        [[nodiscard]] constexpr CodeType code() const {
            return _code;
        }

        // Returns an empty string when no display name is available.
        [[nodiscard]] std::string name() const;
    };

    using KeyID = CodeID<KeyCode>;
    using MouseID = CodeID<MouseCode>;

    template<>
    std::string CodeID<KeyCode>::name() const;

    template<>
    std::string CodeID<MouseCode>::name() const;

    namespace abstract {
        template<typename T>
        struct Input {
            virtual ~Input() = default;

            virtual T state() = 0;
        };

        struct Updatable {
            virtual ~Updatable() = default;

            virtual void update() {};
        };

        struct Adaptable {
        };

        struct Button : public Input<ButtonState> {
            virtual bool down() = 0;

            virtual bool up() = 0;

            virtual bool pressed() = 0;
        };

        struct Cursor : public Input<d2::PointD> {
            virtual d2::VectorD delta() = 0;
        };

        struct Text : public Input<std::string> {
        };

        struct Characters : public Input<std::unordered_set<KeyCode>> {
        };
    } // namespace abstract
} // namespace AHO_NAMESPACE::input

#endif //AHO_ALL_INPUT_HPP
