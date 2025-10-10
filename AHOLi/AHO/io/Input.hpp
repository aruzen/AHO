//
// Created by morimoto_hibiki on 2025/06/03.
//

#ifndef AHO_ALL_INPUT_HPP
#define AHO_ALL_INPUT_HPP

#include "../define.h"

#include <string>
#include <vector>
#include <unordered_set>
#include <array>

#include "../core/Point.h"
#include "../core/Vector.h"


namespace aho{
    struct InputManager;
}
namespace AHO_NAMESPACE::input {
    enum class KeyCode {
        Space = 32,
        Apostrophe = 39,
        Comma = 44,
        Minus = 45,
        Period = 46,
        Slash = 47,
        Num0 = 48,
        Num1 = 49,
        Num2 = 50,
        Num3 = 51,
        Num4 = 52,
        Num5 = 53,
        Num6 = 54,
        Num7 = 55,
        Num8 = 56,
        Num9 = 57,
        Semicolon = 59,
        Equal = 61,
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
        LeftBracket = 91,
        Backslash = 92,
        RightBracket = 93,
        GraveAccent = 96,
        World1 = 161,
        World2 = 162,
        Escape = 256,
        Enter = 257,
        Tab = 258,
        Backspace = 259,
        Insert = 260,
        Delete = 261,
        Right = 262,
        Left = 263,
        Down = 264,
        Up = 265,
        PageUp = 266,
        PageDown = 267,
        Home = 268,
        End = 269,
        CapsLock = 280,
        ScrollLock = 281,
        NumLock = 282,
        PrintScreen = 283,
        Pause = 284,
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
        KeyPad0 = 320,
        KeyPad1 = 321,
        KeyPad2 = 322,
        KeyPad3 = 323,
        KeyPad4 = 324,
        KeyPad5 = 325,
        KeyPad6 = 326,
        KeyPad7 = 327,
        KeyPad8 = 328,
        KeyPad9 = 329,
        KeyPadDecimal = 330,
        KeyPadDivide = 331,
        KeyPadMultiply = 332,
        KeyPadSubtract = 333,
        KeyPadAdd = 334,
        KeyPadEnter = 335,
        KeyPadEqual = 336,
        LeftShift = 340,
        LeftControl = 341,
        LeftAlt = 342,
        LeftSuper = 343,
        RightShift = 344,
        RightControl = 345,
        RightAlt = 346,
        RightSuper = 347,
        Menu = 348,
    };

    enum class MouseCode {
        Button1 = 0,
        Button2 = 1,
        Button3 = 2,
        Button4 = 3,
        Button5 = 4,
        Button6 = 5,
        Button7 = 6,
        Button8 = 7,
        LastButton = Button8,
        LeftClick = Button1,
        RightClick = Button2,
        MiddleButton = Button3,
    };

    constexpr std::array<KeyCode, 120> KeyCodes = {
            KeyCode::Space,
            KeyCode::Apostrophe,
            KeyCode::Comma,
            KeyCode::Minus,
            KeyCode::Period,
            KeyCode::Slash,
            KeyCode::Num0,
            KeyCode::Num1,
            KeyCode::Num2,
            KeyCode::Num3,
            KeyCode::Num4,
            KeyCode::Num5,
            KeyCode::Num6,
            KeyCode::Num7,
            KeyCode::Num8,
            KeyCode::Num9,
            KeyCode::Semicolon,
            KeyCode::Equal,
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
            KeyCode::LeftBracket,
            KeyCode::Backslash,
            KeyCode::RightBracket,
            KeyCode::GraveAccent,
            KeyCode::World1,
            KeyCode::World2,
            KeyCode::Escape,
            KeyCode::Enter,
            KeyCode::Tab,
            KeyCode::Backspace,
            KeyCode::Insert,
            KeyCode::Delete,
            KeyCode::Right,
            KeyCode::Left,
            KeyCode::Down,
            KeyCode::Up,
            KeyCode::PageUp,
            KeyCode::PageDown,
            KeyCode::Home,
            KeyCode::End,
            KeyCode::CapsLock,
            KeyCode::ScrollLock,
            KeyCode::NumLock,
            KeyCode::PrintScreen,
            KeyCode::Pause,
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
            KeyCode::KeyPad0,
            KeyCode::KeyPad1,
            KeyCode::KeyPad2,
            KeyCode::KeyPad3,
            KeyCode::KeyPad4,
            KeyCode::KeyPad5,
            KeyCode::KeyPad6,
            KeyCode::KeyPad7,
            KeyCode::KeyPad8,
            KeyCode::KeyPad9,
            KeyCode::KeyPadDecimal,
            KeyCode::KeyPadDivide,
            KeyCode::KeyPadMultiply,
            KeyCode::KeyPadSubtract,
            KeyCode::KeyPadAdd,
            KeyCode::KeyPadEnter,
            KeyCode::KeyPadEqual,
            KeyCode::LeftShift,
            KeyCode::LeftControl,
            KeyCode::LeftAlt,
            KeyCode::LeftSuper,
            KeyCode::RightShift,
            KeyCode::RightControl,
            KeyCode::RightAlt,
            KeyCode::RightSuper,
            KeyCode::Menu,
    };

    constexpr std::array<MouseCode, 8> MouseCodes{
            MouseCode::LeftClick,
            MouseCode::RightClick,
            MouseCode::MiddleButton,
            MouseCode::Button4,
            MouseCode::Button5,
            MouseCode::Button6,
            MouseCode::Button7,
            MouseCode::LastButton,
    };

    enum class ButtonState {
        Idle, Pressed, Down, Up
    };

    template<typename CodeType>
    class CodeID {
        CodeType _code;
    public:
        constexpr explicit CodeID(int code) : _code((KeyCode) code) {}

        constexpr explicit CodeID(CodeType code) : _code(code) {}

        [[nodiscard]] constexpr CodeType code() const {
            return _code;
        }

        [[nodiscard]] constexpr std::string name() const;
    };

    using KeyID = CodeID<KeyCode>;
    using MouseID = CodeID<MouseCode>;

    namespace abstract {
        template<typename T>
        struct Input {
            virtual T state() = 0;
        };

        struct Updatable {
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
    }
}


#endif //AHO_ALL_INPUT_HPP
