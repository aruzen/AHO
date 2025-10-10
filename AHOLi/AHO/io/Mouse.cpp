//
// Created by morimoto_hibiki on 2025/09/22.
//

#include "Mouse.hpp"

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

aho::input::Cursor::Cursor(void *context) : context(context) {
    glfwGetCursorPos((GLFWwindow *) context, &_state.value.x.value, &_state.value.y.value);
}

aho::d2::PointD aho::input::Cursor::state() {
    return _state;
}

aho::d2::VectorD aho::input::Cursor::delta() {
    return _delta;
}

void aho::input::Cursor::update() {
    _delta.value = _state.value;
    glfwGetCursorPos((GLFWwindow *) context, &_state.value.x.value, &_state.value.y.value);
    _delta.value.x -= _state.value.x;
    _delta.value.y -= _state.value.y;
}

aho::input::ButtonState aho::input::MouseButton::state() {
    return _state;
}

bool aho::input::MouseButton::down() {
    return _state == ButtonState::Down;
}

bool aho::input::MouseButton::up() {
    return _state == ButtonState::Up;
}

bool aho::input::MouseButton::pressed() {
    return _state == ButtonState::Down || _state == ButtonState::Pressed;
}

void aho::input::MouseButton::update() {
    switch (glfwGetMouseButton((GLFWwindow *) context, (int) code.code())) {
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

aho::input::MouseButton::MouseButton(void *context, aho::input::MouseCode code) : context(context), code(code) {}

aho::input::MouseButton::MouseButton(void *context, aho::input::MouseID code) : context(context), code(code) {}

size_t aho::input::MouseWheel::subscribed = 0;

aho::d2::VectorD aho::input::MouseWheel::_state = aho::d2::VectorD(0.0, 0.0);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    aho::input::MouseWheel::_state = aho::d2::VectorD(xoffset, yoffset);
}

aho::input::MouseWheel::MouseWheel(void *context) : context(context) {
    if (subscribed == 0)
        glfwSetScrollCallback((GLFWwindow *) context, scroll_callback);
    subscribed++;
}

aho::input::MouseWheel::~MouseWheel() {
    subscribed--;
    if (subscribed == 0)
        glfwSetScrollCallback((GLFWwindow *) context, NULL);
}

aho::d2::VectorD aho::input::MouseWheel::state() {
    return _state;
}

aho::input::Mouse::Mouse(void *, aho::InputManager &manager) {
    _cursor = manager.get<Cursor>();
    _rightClick = manager.get<MouseButton>(MouseCode::RightClick);
    _middleButton = manager.get<MouseButton>(MouseCode::MiddleButton);
    _leftClick = manager.get<MouseButton>(MouseCode::LeftClick);
    _wheel = manager.get<MouseWheel>();
}

std::shared_ptr<aho::input::Cursor> aho::input::Mouse::cursor() {
    return _cursor;
}

std::shared_ptr<aho::input::MouseButton> aho::input::Mouse::rightClick() {
    return _rightClick;
}

std::shared_ptr<aho::input::MouseButton> aho::input::Mouse::middleButton() {
    return _middleButton;
}

std::shared_ptr<aho::input::MouseButton> aho::input::Mouse::leftClick() {
    return _leftClick;
}

std::shared_ptr<aho::input::MouseWheel> aho::input::Mouse::wheel() {
    return _wheel;
}

