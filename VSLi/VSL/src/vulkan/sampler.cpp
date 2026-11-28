//
// Created by morimoto_hibiki on 2025/10/21.
//

#include <VSL/vulkan/sampler.hpp>

#include <VSL/vulkan/_pimpls.hpp>

VSL_NAMESPACE::Sampler::Sampler(LogicalDeviceAccessor device, SamplingMode sampling, EdgeMode edge)
    : Sampler(static_cast<Arena*>(nullptr), device, sampling, edge) {}

VSL_NAMESPACE::Sampler::Sampler(Arena& arena, LogicalDeviceAccessor device, SamplingMode sampling, EdgeMode edge)
    : Sampler(&arena, device, sampling, edge) {}

VSL_NAMESPACE::Sampler::Sampler(Arena* arena, LogicalDeviceAccessor device, SamplingMode sampling, EdgeMode edge) {
    data_ = place<impl::SamplerImpl>(arena, [&](impl::SamplerImpl& sampler) {
        sampler.device = device.data_;

        vk::SamplerCreateInfo sampler_info;
        sampler_info.magFilter = (vk::Filter)sampling;
        sampler_info.minFilter = (vk::Filter)sampling;
        sampler_info.addressModeU = (vk::SamplerAddressMode)edge;
        sampler_info.addressModeV = (vk::SamplerAddressMode)edge;
        sampler_info.addressModeW = (vk::SamplerAddressMode)edge;

        sampler_info.anisotropyEnable = sampling != SamplingMode::NEAREST;
        if (not device.data_->parent_device->props)
            device.data_->parent_device->make_props();
        sampler_info.maxAnisotropy = device.data_->parent_device->props->limits.maxSamplerAnisotropy;

        // FIXME: Formatで変えないと
        sampler_info.borderColor = vk::BorderColor::eFloatTransparentBlack;
        sampler_info.unnormalizedCoordinates = false;

        sampler_info.compareEnable = false;
        sampler_info.compareOp = vk::CompareOp::eAlways;

        sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;

        sampler.sampler = device.data_->device.createSampler(sampler_info);
    });
}

VSL_NAMESPACE::impl::SamplerImpl::~SamplerImpl() {
    device->device.destroy(sampler);
}
