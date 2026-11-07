#pragma once
#include "../define.hpp"

namespace VSL_NAMESPACE {
    enum class SamplingMode {
        NEAREST = 0,
        LINEAR = 1,
    };

    enum class EdgeMode {
        REPEAT = 0,
        MIRRORED_REPEAT = 1,
        CLAMP_TO_EDGE = 2,
        CLAMP_TO_BORDER = 3,
        MIRROR_CLAMP_TO_EDGE = 4,
    };

    struct SamplerCreateInfo {
        SamplingMode sampling = SamplingMode::LINEAR;
        EdgeMode edge = EdgeMode::CLAMP_TO_BORDER;
    };
} // namespace VSL_NAMESPACE
