//
// Created by morimoto_hibiki on 2025/09/22.
//

#include <AHO/io/mouse.hpp>

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <unordered_map>

struct AHO_NAMESPACE::input::Cursor::CaptureState {
    std::size_t generation = 0;
};

std::shared_ptr<AHO_NAMESPACE::input::Cursor::CaptureState> AHO_NAMESPACE::input::Cursor::GetCaptureState(void* context) {
    static std::unordered_map<void*, std::weak_ptr<CaptureState>> STATES;
    std::erase_if(STATES, [](const auto& entry) { return entry.second.expired(); });
    auto state = STATES[context].lock();
    if (!state) {
        state = std::make_shared<CaptureState>();
        STATES[context] = state;
    }
    return state;
}

AHO_NAMESPACE::input::Cursor::Cursor(void* context)
    : _context(context), _capture_state(GetCaptureState(context)), _capture_generation(_capture_state->generation) {
    glfwGetCursorPos((GLFWwindow*)context, &_state.value.x.value, &_state.value.y.value);
    _previous_focus = focused();
}

AHO_NAMESPACE::d2::PointD AHO_NAMESPACE::input::Cursor::state() {
    return _state;
}

AHO_NAMESPACE::d2::VectorD AHO_NAMESPACE::input::Cursor::delta() {
    if (_capture_generation != _capture_state->generation)
        return d2::VectorD(0.0, 0.0);
    return _delta;
}

void AHO_NAMESPACE::input::Cursor::update() {
    _delta.value = _state.value;
    glfwGetCursorPos((GLFWwindow*)_context, &_state.value.x.value, &_state.value.y.value);
    _delta.value.x = _state.value.x - _delta.value.x;
    _delta.value.y = _state.value.y - _delta.value.y;
    const bool focus = focused();
    if (!focus || !_previous_focus || _capture_generation != _capture_state->generation)
        _delta = d2::VectorD(0.0, 0.0);
    _previous_focus = focus;
    _capture_generation = _capture_state->generation;
}

void AHO_NAMESPACE::input::Cursor::set_captured(bool capture) {
    auto* window = static_cast<GLFWwindow*>(_context);
    const int mode = capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    if (glfwGetInputMode(window, GLFW_CURSOR) == mode)
        return;
    glfwSetInputMode(window, GLFW_CURSOR, mode);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, capture ? GLFW_TRUE : GLFW_FALSE);
    ++_capture_state->generation;
    _delta = d2::VectorD(0.0, 0.0);
}

bool AHO_NAMESPACE::input::Cursor::captured() const {
    return glfwGetInputMode(static_cast<GLFWwindow*>(_context), GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

bool AHO_NAMESPACE::input::Cursor::focused() const {
    return glfwGetWindowAttrib(static_cast<GLFWwindow*>(_context), GLFW_FOCUSED) == GLFW_TRUE;
}

AHO_NAMESPACE::input::ButtonState AHO_NAMESPACE::input::MouseButton::state() {
    return _state;
}

bool AHO_NAMESPACE::input::MouseButton::down() {
    return _state == ButtonState::DOWN;
}

bool AHO_NAMESPACE::input::MouseButton::up() {
    return _state == ButtonState::UP;
}

bool AHO_NAMESPACE::input::MouseButton::pressed() {
    return _state == ButtonState::DOWN || _state == ButtonState::PRESSED;
}

void AHO_NAMESPACE::input::MouseButton::update() {
    switch (glfwGetMouseButton((GLFWwindow*)_context, (int)_code.code())) {
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

AHO_NAMESPACE::input::MouseButton::MouseButton(void* context, AHO_NAMESPACE::input::MouseCode code) : _context(context), _code(code) {}

AHO_NAMESPACE::input::MouseButton::MouseButton(void* context, AHO_NAMESPACE::input::MouseID code) : _context(context), _code(code) {}

size_t AHO_NAMESPACE::input::MouseWheel::SUBSCRIBED = 0;

size_t AHO_NAMESPACE::input::MouseWheel::GENERATION = 0;

AHO_NAMESPACE::d2::VectorD AHO_NAMESPACE::input::MouseWheel::STATE = AHO_NAMESPACE::d2::VectorD(0.0, 0.0);

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    AHO_NAMESPACE::input::MouseWheel::STATE = AHO_NAMESPACE::d2::VectorD(xoffset, yoffset);
    AHO_NAMESPACE::input::MouseWheel::GENERATION++;
}

AHO_NAMESPACE::input::MouseWheel::MouseWheel(void* context) : _context(context) {
    _gen = GENERATION;
    _read = GENERATION;
    if (SUBSCRIBED == 0)
        glfwSetScrollCallback((GLFWwindow*)context, scroll_callback);
    SUBSCRIBED++;
}

AHO_NAMESPACE::input::MouseWheel::MouseWheel(const MouseWheel& other) noexcept : MouseWheel(other._context) {
    _gen = other._gen;
    _read = other._read;
}

AHO_NAMESPACE::input::MouseWheel::~MouseWheel() {
    SUBSCRIBED--;
    if (SUBSCRIBED == 0)
        glfwSetScrollCallback((GLFWwindow*)_context, NULL);
}

void AHO_NAMESPACE::input::MouseWheel::update() {
    if (_gen < _read)
        _gen = _read;
    if (_gen + 60 < GENERATION)
        _gen = GENERATION - 60;
}

AHO_NAMESPACE::d2::VectorD AHO_NAMESPACE::input::MouseWheel::state() {
    if (_gen < GENERATION) {
        _read = GENERATION;
        return STATE;
    } else
        return {};
}

AHO_NAMESPACE::input::Mouse::Mouse(void*, AHO_NAMESPACE::InputManager& manager) {
    _cursor = manager.get<Cursor>();
    _right_click = manager.get<MouseButton>(MouseCode::RIGHT_CLICK);
    _middle_button = manager.get<MouseButton>(MouseCode::MIDDLE_BUTTON);
    _left_click = manager.get<MouseButton>(MouseCode::LEFT_CLICK);
    _wheel = manager.get<MouseWheel>();
}

std::shared_ptr<AHO_NAMESPACE::input::Cursor> AHO_NAMESPACE::input::Mouse::cursor() {
    return _cursor;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseButton> AHO_NAMESPACE::input::Mouse::right_click() {
    return _right_click;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseButton> AHO_NAMESPACE::input::Mouse::middle_button() {
    return _middle_button;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseButton> AHO_NAMESPACE::input::Mouse::left_click() {
    return _left_click;
}

std::shared_ptr<AHO_NAMESPACE::input::MouseWheel> AHO_NAMESPACE::input::Mouse::wheel() {
    return _wheel;
}
