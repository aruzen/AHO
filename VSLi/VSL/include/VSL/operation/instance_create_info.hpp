#pragma once
#include "../define.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace VSL_NAMESPACE {
    struct InstanceCreateInfo {
        std::string application_name = "VSL";
        std::uint32_t application_version = 1u << 22;
        std::string engine_name = "VSLEngine";
        std::uint32_t engine_version = 1u << 22;
        std::uint32_t api_version = (1u << 22) | (3u << 12);
        std::vector<std::string> extensions, layers;
        bool validation = false;
        bool debug_printf = false;
    };
} // namespace VSL_NAMESPACE
