//
// Created by morimoto_hibiki on 2025/06/03.
//

#include "Input.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

constexpr std::string aho::input::KeyID::name() const {
    return std::string(glfwGetKeyName((int)_code, NULL));
}
