//
// Created by morimoto_hibiki on 2025/10/21.
//

#ifndef AHO_ALL_SAMPLER_H
#define AHO_ALL_SAMPLER_H

#include "image.hpp"
#include "../operation/sampler_create_info.hpp"

namespace VSL_NAMESPACE {
    namespace lifetime::detail {
        struct SamplerAccess;
    }
    struct SamplerAccessor {
        SamplerAccessor() = default;
        explicit operator bool() const { return bool(_owner); }
        ResourceUseState& use_state() const;
        ResourceUse resource_use() const;

    protected:
        SamplerAccessor(LogicalDeviceAccessor device, const SamplerCreateInfo& info);

    private:
        friend struct lifetime::detail::SamplerAccess;
        lifetime::OwnedRef<impl::SamplerImpl> _owner;
        explicit SamplerAccessor(lifetime::OwnedRef<impl::SamplerImpl> owner) : _owner(std::move(owner)) {}
    };

    struct Sampler : public SamplerAccessor {
        Sampler(LogicalDeviceAccessor device, SamplingMode sampling = SamplingMode::LINEAR, EdgeMode edge = EdgeMode::CLAMP_TO_BORDER);
    };
} // namespace VSL_NAMESPACE

#endif //AHO_ALL_SAMPLER_H
