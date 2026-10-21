//
// Created by morimoto_hibiki on 2025/06/03.
//

#include <AHO/io/input.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

template<>
std::string AHO_NAMESPACE::input::CodeID<AHO_NAMESPACE::input::KeyCode>::name() const {
    const char* name = glfwGetKeyName(static_cast<int>(_code), 0);
    return name ? name : "";
}

template<>
std::string AHO_NAMESPACE::input::CodeID<AHO_NAMESPACE::input::MouseCode>::name() const {
    switch (_code) {
        case MouseCode::LEFT_CLICK:
            return "LeftClick";
        case MouseCode::RIGHT_CLICK:
            return "RightClick";
        case MouseCode::MIDDLE_BUTTON:
            return "MiddleButton";
        case MouseCode::BUTTON4:
            return "Button4";
        case MouseCode::BUTTON5:
            return "Button5";
        case MouseCode::BUTTON6:
            return "Button6";
        case MouseCode::BUTTON7:
            return "Button7";
        case MouseCode::BUTTON8:
            return "Button8";
    }
    return {};
}
