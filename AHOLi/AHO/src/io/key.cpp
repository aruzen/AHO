//
// Created by morimoto_hibiki on 2025/06/03.
//

#include <AHO/io/key.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

AHO_NAMESPACE::input::Key::Key(void* context, AHO_NAMESPACE::input::KeyCode code) : context(context), _code(code) {}

AHO_NAMESPACE::input::Key::Key(void* context, AHO_NAMESPACE::input::KeyID code) : context(context), _code(code) {}

AHO_NAMESPACE::input::ButtonState AHO_NAMESPACE::input::Key::state() {
    return _state;
}

bool AHO_NAMESPACE::input::Key::down() {
    return _state == ButtonState::Down;
}

bool AHO_NAMESPACE::input::Key::up() {
    return _state == ButtonState::Up;
}

/*
 * 押されている状態ならtrue(ButtonState::Down or ButtonState::Pressed)
 */
bool AHO_NAMESPACE::input::Key::pressed() {
    return _state == ButtonState::Down || _state == ButtonState::Pressed;
}

void AHO_NAMESPACE::input::Key::update() {
    switch (glfwGetKey((GLFWwindow*)context, (int)_code.code())) {
        default:
        case GLFW_RELEASE:
            if (pressed())
                _state = ButtonState::Up;
            else
                _state = ButtonState::Idle;
            break;
        case GLFW_PRESS:
            if (pressed())
                _state = ButtonState::Pressed;
            else
                _state = ButtonState::Down;
    }
}

AHO_NAMESPACE::input::KeyID AHO_NAMESPACE::input::Key::code() {
    return _code;
}
