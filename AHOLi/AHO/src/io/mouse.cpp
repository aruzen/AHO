//
// Created by morimoto_hibiki on 2025/09/22.
//

#include <AHO/io/mouse.hpp>

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

AHO_NAMESPACE::input::Cursor::Cursor(void* context) : context(context) {
    glfwGetCursorPos((GLFWwindow*)context, &_state.value.x.value, &_state.value.y.value);
    previousFocus = focused();
}

AHO_NAMESPACE::d2::PointD AHO_NAMESPACE::input::Cursor::state() {
    return _state;
}

AHO_NAMESPACE::d2::VectorD AHO_NAMESPACE::input::Cursor::delta() {
    return _delta;
}

void AHO_NAMESPACE::input::Cursor::update() {
    _delta.value = _state.value;
    glfwGetCursorPos((GLFWwindow*)context, &_state.value.x.value, &_state.value.y.value);
    _delta.value.x = _state.value.x - _delta.value.x;
    _delta.value.y = _state.value.y - _delta.value.y;
    const bool focus = focused();
    if (!focus || !previousFocus || skipDelta)
        _delta = d2::VectorD(0.0, 0.0);
    previousFocus = focus;
    skipDelta = false;
}

void AHO_NAMESPACE::input::Cursor::setCaptured(bool capture) {
    auto* window = static_cast<GLFWwindow*>(context);
    glfwSetInputMode(window, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, capture ? GLFW_TRUE : GLFW_FALSE);
    skipDelta = true;
    _delta = d2::VectorD(0.0, 0.0);
}

bool AHO_NAMESPACE::input::Cursor::captured() const {
    return glfwGetInputMode(static_cast<GLFWwindow*>(context), GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

bool AHO_NAMESPACE::input::Cursor::focused() const {
    return glfwGetWindowAttrib(static_cast<GLFWwindow*>(context), GLFW_FOCUSED) == GLFW_TRUE;
}

AHO_NAMESPACE::input::ButtonState AHO_NAMESPACE::input::MouseButton::state() {
    return _state;
}

bool AHO_NAMESPACE::input::MouseButton::down() {
    return _state == ButtonState::Down;
}

bool AHO_NAMESPACE::input::MouseButton::up() {
    return _state == ButtonState::Up;
}

bool AHO_NAMESPACE::input::MouseButton::pressed() {
    return _state == ButtonState::Down || _state == ButtonState::Pressed;
}

void AHO_NAMESPACE::input::MouseButton::update() {
    switch (glfwGetMouseButton((GLFWwindow*)context, (int)code.code())) {
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

AHO_NAMESPACE::input::MouseButton::MouseButton(void* context, AHO_NAMESPACE::input::MouseCode code) : context(context), code(code) {}

AHO_NAMESPACE::input::MouseButton::MouseButton(void* context, AHO_NAMESPACE::input::MouseID code) : context(context), code(code) {}

size_t AHO_NAMESPACE::input::MouseWheel::subscribed = 0;

size_t AHO_NAMESPACE::input::MouseWheel::_generation = 0;

AHO_NAMESPACE::d2::VectorD AHO_NAMESPACE::input::MouseWheel::_state = AHO_NAMESPACE::d2::VectorD(0.0, 0.0);

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    AHO_NAMESPACE::input::MouseWheel::_state = AHO_NAMESPACE::d2::VectorD(xoffset, yoffset);
    AHO_NAMESPACE::input::MouseWheel::_generation++;
}

AHO_NAMESPACE::input::MouseWheel::MouseWheel(void* context) : context(context) {
    gen = _generation;
    read = _generation;
    if (subscribed == 0)
        glfwSetScrollCallback((GLFWwindow*)context, scroll_callback);
    subscribed++;
}

AHO_NAMESPACE::input::MouseWheel::MouseWheel(const MouseWheel& other) noexcept : MouseWheel(other.context) {
    gen = other.gen;
    read = other.read;
}

AHO_NAMESPACE::input::MouseWheel::~MouseWheel() {
    subscribed--;
    if (subscribed == 0)
        glfwSetScrollCallback((GLFWwindow*)context, NULL);
}

void AHO_NAMESPACE::input::MouseWheel::update() {
    if (gen < read)
        gen = read;
    if (gen + 60 < _generation)
        gen = _generation - 60;
}

AHO_NAMESPACE::d2::VectorD AHO_NAMESPACE::input::MouseWheel::state() {
    if (gen < _generation) {
        read = _generation;
        return _state;
    } else
        return {};
}

AHO_NAMESPACE::input::Mouse::Mouse(void*, AHO_NAMESPACE::InputManager& manager) {
    _cursor = manager.get<Cursor>();
    _rightClick = manager.get<MouseButton>(MouseCode::RightClick);
    _middleButton = manager.get<MouseButton>(MouseCode::MiddleButton);
    _leftClick = manager.get<MouseButton>(MouseCode::LeftClick);
    _wheel = manager.get<MouseWheel>();
}

std::shared_ptr<AHO_NAMESPACE::input::Cursor> AHO_NAMESPACE::input::Mouse::cursor() {
    return _cursor;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseButton> AHO_NAMESPACE::input::Mouse::rightClick() {
    return _rightClick;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseButton> AHO_NAMESPACE::input::Mouse::middleButton() {
    return _middleButton;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseButton> AHO_NAMESPACE::input::Mouse::leftClick() {
    return _leftClick;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseWheel> AHO_NAMESPACE::input::Mouse::wheel() {
    return _wheel;
}
