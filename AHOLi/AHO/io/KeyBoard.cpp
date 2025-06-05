//
// Created by morimoto_hibiki on 2025/06/04.
//

#include "KeyBoard.hpp"


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <unordered_set>

int KEYBOARD_TRACKED = 0;

std::unordered_set<aho::input::KeyCode> PRESSED_KEYS;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        PRESSED_KEYS.insert((aho::input::KeyCode)key);
    } else if (action == GLFW_RELEASE) {
        PRESSED_KEYS.erase((aho::input::KeyCode)key);
    }
}

aho::input::KeyBoard::KeyBoard(void *context) {
    if (KEYBOARD_TRACKED == 0)
        glfwSetKeyCallback((GLFWwindow*)context, key_callback);
    KEYBOARD_TRACKED++;
}

std::unordered_set<aho::input::KeyCode> aho::input::KeyBoard::state() {
    return PRESSED_KEYS;
}

aho::input::KeyBoard::~KeyBoard() {
    KEYBOARD_TRACKED--;
    if (KEYBOARD_TRACKED == 0) {
        glfwSetKeyCallback((GLFWwindow *) context, nullptr);
        PRESSED_KEYS.clear();
    }
}

bool aho::input::KeyBoard::pressed(aho::input::KeyCode code) const {
    return PRESSED_KEYS.contains(code);
}
