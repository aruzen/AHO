//
// Created by morimoto_hibiki on 2025/06/03.
//

#include <AHO/io/input.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

template<>
constexpr std::string aho::input::CodeID<aho::input::KeyCode>::name() const {
    return { glfwGetKeyName((int)_code, (int)NULL) };
}

template<>
constexpr std::string aho::input::CodeID<aho::input::MouseCode>::name() const {
    switch (_code) {
        case MouseCode::LeftClick:
            return "LeftClick";
        case MouseCode::RightClick:
            return "RightClick";
        case MouseCode::MiddleButton:
            return "MiddleButton";
        case MouseCode::Button4:
            return "Button4";
        case MouseCode::Button5:
            return "Button5";
        case MouseCode::Button6:
            return "Button6";
        case MouseCode::Button7:
            return "Button7";
        case MouseCode::Button8:
            return "Button8";
    }
}
