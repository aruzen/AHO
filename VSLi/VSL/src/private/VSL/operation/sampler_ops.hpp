#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/sampler_create_info.hpp>

namespace VSL_NAMESPACE::operation {
    void initialize_sampler(impl::SamplerImpl& target, impl::LogicalDeviceImpl& device, const SamplerCreateInfo& info);
    void destroy_sampler(impl::SamplerImpl& target) noexcept;
} // namespace VSL_NAMESPACE::operation
