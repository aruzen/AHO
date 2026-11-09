#include <VSL/lifetime/sampler_access.hpp>
#include <VSL/vulkan/sampler.hpp>
#include <VSL/lifetime/resource_use_factory.hpp>
#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::SamplerAccess;
}

namespace VSL_NAMESPACE {
    Sampler::Sampler(LogicalDeviceAccessor device, SamplingMode sampling, EdgeMode edge)
        : SamplerAccessor(std::move(device), {sampling, edge}) {}
    SamplerAccessor::SamplerAccessor(LogicalDeviceAccessor device, const SamplerCreateInfo& info)
        : SamplerAccessor(SamplerAccess::FromShared(lifetime::make_heap_sampler(std::move(device), info))) {}
    SamplerAccessor lifetime::detail::SamplerAccess::FromShared(std::shared_ptr<impl::SamplerImpl> owner) {
        const auto* node = owner ? owner->owner_node : nullptr;
        return FromOwned(owned_ref(std::move(owner), node));
    }
    ResourceUseState& SamplerAccessor::use_state() const {
        if (!_owner || !_owner->use)
            throw std::invalid_argument("Sampler is empty");
        return *_owner->use;
    }
    ResourceUse SamplerAccessor::resource_use() const {
        return lifetime::resource_use_for_sampler(_owner);
    }
} // namespace VSL_NAMESPACE
