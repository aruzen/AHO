#pragma once
#include <VSL/vulkan/sampler.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct SamplerAccess {
        static const OwnedRef<impl::SamplerImpl>& Owned(const SamplerAccessor& sampler) { return sampler._owner; }
        static OwnedRef<impl::SamplerImpl>& Owned(SamplerAccessor& sampler) { return sampler._owner; }
        static OwnedRef<impl::SamplerImpl> Owned(SamplerAccessor&& sampler) { return std::move(sampler._owner); }
        static OwnedRef<impl::SamplerImpl> Owned(const SamplerAccessor&& sampler) { return sampler._owner; }
        static SamplerAccessor FromOwned(OwnedRef<impl::SamplerImpl> owner) { return SamplerAccessor(std::move(owner)); }
        static SamplerAccessor FromShared(std::shared_ptr<impl::SamplerImpl> owner);
    };
} // namespace VSL_NAMESPACE::lifetime::detail
