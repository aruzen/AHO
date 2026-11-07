#pragma once
#include <VSL/operation/sampler_ops.hpp>
#include <VSL/lifetime/resource_use.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct SamplerSlotStorage {
        alignas(impl::SamplerImpl) std::byte sampler[sizeof(impl::SamplerImpl)];
        alignas(ResourceUseState) std::byte use[sizeof(ResourceUseState)];
    };
    void construct_sampler(void* storage, LogicalDeviceAccessor device, const SamplerCreateInfo& info);
    void destroy_sampler(void* storage) noexcept;
    impl::SamplerImpl& sampler_at(void* storage);
    std::shared_ptr<impl::SamplerImpl> make_heap_sampler(LogicalDeviceAccessor device, const SamplerCreateInfo& info);
} // namespace VSL_NAMESPACE::lifetime
