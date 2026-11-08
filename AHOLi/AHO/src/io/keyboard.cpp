//
// Created by morimoto_hibiki on 2025/06/04.
//

#include <AHO/io/keyboard.hpp>
#include <VSL/lifetime/glfw_lease.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <unordered_set>

int KEYBOARD_TRACKED = 0;

std::unordered_set<AHO_NAMESPACE::input::KeyCode> PRESSED_KEYS;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    VSL_NAMESPACE::lifetime::GLFWCallbackScope scope;
    if (action == GLFW_PRESS) {
        PRESSED_KEYS.insert((AHO_NAMESPACE::input::KeyCode)key);
    } else if (action == GLFW_RELEASE) {
        PRESSED_KEYS.erase((AHO_NAMESPACE::input::KeyCode)key);
    }
}

AHO_NAMESPACE::input::KeyBoard::KeyBoard(void* context) : _context(context) {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope;
    if (KEYBOARD_TRACKED == 0)
        glfwSetKeyCallback((GLFWwindow*)context, key_callback);
    KEYBOARD_TRACKED++;
}

AHO_NAMESPACE::input::KeyBoard::KeyBoard(const KeyBoard& other) noexcept : abstract::Characters(other), _context(other._context) {
    ++KEYBOARD_TRACKED;
}

std::unordered_set<AHO_NAMESPACE::input::KeyCode> AHO_NAMESPACE::input::KeyBoard::state() {
    return PRESSED_KEYS;
}

AHO_NAMESPACE::input::KeyBoard::~KeyBoard() {
    VSL_NAMESPACE::lifetime::GLFWCallScope scope(std::nothrow);
    KEYBOARD_TRACKED--;
    if (KEYBOARD_TRACKED == 0) {
        glfwSetKeyCallback((GLFWwindow*)_context, nullptr);
        PRESSED_KEYS.clear();
    }
}

bool AHO_NAMESPACE::input::KeyBoard::pressed(AHO_NAMESPACE::input::KeyCode code) const {
    return PRESSED_KEYS.contains(code);
}
