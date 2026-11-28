//
// Created by morimoto_hibiki on 2025/10/21.
//

#ifndef AHO_ALL_SAMPLER_H
#define AHO_ALL_SAMPLER_H

#include "image.hpp"
#include "../arena.hpp"

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

    struct SamplerAccessor {
        std::shared_ptr<impl::SamplerImpl> data_;
    };

    struct Sampler : public SamplerAccessor {
        Sampler(LogicalDeviceAccessor device, SamplingMode sampling = SamplingMode::LINEAR, EdgeMode edge = EdgeMode::CLAMP_TO_BORDER);
        // Implをarenaへ配置する。寿命はarena全体と共有する。
        Sampler(Arena& arena, LogicalDeviceAccessor device, SamplingMode sampling = SamplingMode::LINEAR, EdgeMode edge = EdgeMode::CLAMP_TO_BORDER);

    private:
        Sampler(Arena* arena, LogicalDeviceAccessor device, SamplingMode sampling, EdgeMode edge);
    };
} // namespace VSL_NAMESPACE

#endif //AHO_ALL_SAMPLER_H
