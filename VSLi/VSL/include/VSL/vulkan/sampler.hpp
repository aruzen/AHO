//
// Created by morimoto_hibiki on 2025/10/21.
//

#ifndef AHO_ALL_SAMPLER_H
#define AHO_ALL_SAMPLER_H

#include "image.hpp"

namespace VSL_NAMESPACE {
    enum class SamplingMode {
        Nearest = 0,
        Linear = 1,
    };

    enum class EdgeMode {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
        MirrorClampToEdge = 4,
    };

    struct SamplerAccessor {
        std::shared_ptr<_impl::Sampler_impl> _data;
    };

    struct Sampler : public SamplerAccessor {
        Sampler(LogicalDeviceAccessor device, SamplingMode sampling = SamplingMode::Linear, EdgeMode edge = EdgeMode::ClampToBorder);
    };
}

#endif //AHO_ALL_SAMPLER_H
