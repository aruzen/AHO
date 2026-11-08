#pragma once
#include "../define.hpp"
#include <cstddef>

namespace VSL_NAMESPACE {
    struct SemaphoreCreateInfo {
        size_t count = 1;
    };
    struct FenceCreateInfo {
        size_t count = 1;
        bool signaled = false;
    };
} // namespace VSL_NAMESPACE
