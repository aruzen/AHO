#include <VSL/vulkan/sampler.hpp>
#include <VSL/lifetime/sampler_storage.hpp>

namespace VSL_NAMESPACE {
    Sampler::Sampler(LogicalDeviceAccessor device, SamplingMode sampling, EdgeMode edge) {
        data_ = lifetime::make_heap_sampler(device, {sampling, edge});
    }
    ResourceUseState& SamplerAccessor::use_state() const {
        if (!data_ || !data_->use)
            throw std::invalid_argument("Sampler is empty");
        return *data_->use;
    }
    ResourceUse SamplerAccessor::resource_use() const {
        return ResourceUse::ForSampler(data_);
    }
} // namespace VSL_NAMESPACE
