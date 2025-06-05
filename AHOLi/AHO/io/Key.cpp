//
// Created by morimoto_hibiki on 2025/06/03.
//

#include "Key.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

aho::input::Key::Key(void *context, aho::input::KeyCode code) : context(context), code(code) {}

aho::input::Key::Key(void *context, aho::input::KeyID code) : context(context), code(code){}

aho::input::ButtonState aho::input::Key::state() {
    return _state;
}

bool aho::input::Key::down() {
    return _state == ButtonState::Down;
}

bool aho::input::Key::up() {
    return _state == ButtonState::Up;
}

/*
 * 押されている状態ならtrue(ButtonState::Down or ButtonState::Pressed)
 */
bool aho::input::Key::pressed() {
    return _state == ButtonState::Down || _state == ButtonState::Pressed;
}

void aho::input::Key::update() {
    switch (glfwGetKey((GLFWwindow*)context, (int)code.code())) {
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