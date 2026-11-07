//
// Created by morimoto_hibiki on 2025/10/21.
//

#ifndef AHO_ALL_SAMPLER_H
#define AHO_ALL_SAMPLER_H

#include "image.hpp"
#include "../operation/sampler_create_info.hpp"

namespace VSL_NAMESPACE {
    struct SamplerAccessor {
        std::shared_ptr<impl::SamplerImpl> data_;
        ResourceUseState& use_state() const;
        ResourceUse resource_use() const;
    };

    struct Sampler : public SamplerAccessor {
        Sampler(LogicalDeviceAccessor device, SamplingMode sampling = SamplingMode::LINEAR, EdgeMode edge = EdgeMode::CLAMP_TO_BORDER);
    };
} // namespace VSL_NAMESPACE

#endif //AHO_ALL_SAMPLER_H
