//
// Created by morimoto_hibiki on 2025/10/21.
//

#include "sampler.h"

#include "_pimpls.h"

vsl::Sampler::Sampler(LogicalDeviceAccessor device, vsl::SamplingMode sampling, vsl::EdgeMode edge) {
    _data = std::shared_ptr<_impl::Sampler_impl>(new _impl::Sampler_impl);
    _data->device = device._data;

    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = (vk::Filter) sampling;
    samplerInfo.minFilter = (vk::Filter) sampling;
    samplerInfo.addressModeU = (vk::SamplerAddressMode) edge;
    samplerInfo.addressModeV = (vk::SamplerAddressMode) edge;
    samplerInfo.addressModeW = (vk::SamplerAddressMode) edge;

    samplerInfo.anisotropyEnable = sampling != SamplingMode::Nearest;
    samplerInfo.maxAnisotropy = device._data->parentDevice->props->limits.maxSamplerAnisotropy;

    // FIXME: Formatで変えないと
    samplerInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
    samplerInfo.unnormalizedCoordinates = false;

    samplerInfo.compareEnable = false;
    samplerInfo.compareOp = vk::CompareOp::eAlways;

    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    _data->sampler = device._data->device.createSampler(samplerInfo);
}

vsl::_impl::Sampler_impl::~Sampler_impl() {
    device->device.destroy(sampler);
}