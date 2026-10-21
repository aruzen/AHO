//
// Created by morimoto_hibiki on 2025/06/03.
//

#include <AHO/io/key.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

AHO_NAMESPACE::input::Key::Key(void* context, AHO_NAMESPACE::input::KeyCode code) : _context(context), _code(code) {}

AHO_NAMESPACE::input::Key::Key(void* context, AHO_NAMESPACE::input::KeyID code) : _context(context), _code(code) {}

AHO_NAMESPACE::input::ButtonState AHO_NAMESPACE::input::Key::state() {
    return _state;
}

bool AHO_NAMESPACE::input::Key::down() {
    return _state == ButtonState::DOWN;
}

bool AHO_NAMESPACE::input::Key::up() {
    return _state == ButtonState::UP;
}

/*
 * 押されている状態ならtrue(ButtonState::DOWN or ButtonState::PRESSED)
 */
bool AHO_NAMESPACE::input::Key::pressed() {
    return _state == ButtonState::DOWN || _state == ButtonState::PRESSED;
}

void AHO_NAMESPACE::input::Key::update() {
    switch (glfwGetKey((GLFWwindow*)_context, (int)_code.code())) {
        default:
        case GLFW_RELEASE:
            if (pressed())
                _state = ButtonState::UP;
            else
                _state = ButtonState::IDLE;
            break;
        case GLFW_PRESS:
            if (pressed())
                _state = ButtonState::PRESSED;
            else
                _state = ButtonState::DOWN;
    }
}

AHO_NAMESPACE::input::KeyID AHO_NAMESPACE::input::Key::code() {
    return _code;
}
