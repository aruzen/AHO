#pragma once
#include "../vulkan/shader.hpp"
#include <string>
#include <vector>

namespace VSL_NAMESPACE {
    // codeとentryは入力自身が所有し、実体構築後も独立して利用できる。
    struct ShaderCreateInfo {
        ShaderType stage = ShaderType::VERTEX;
        std::vector<std::uint32_t> code;
        std::string name = "main";
    };
} // namespace VSL_NAMESPACE
