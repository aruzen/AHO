#include <VSL/operation/sampler_ops.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    void initialize_sampler(impl::SamplerImpl& target, impl::LogicalDeviceImpl& device, const SamplerCreateInfo& info) {
        try {
            if (!device.device || (info.sampling != SamplingMode::NEAREST && info.sampling != SamplingMode::LINEAR) ||
                static_cast<unsigned int>(info.edge) > static_cast<unsigned int>(EdgeMode::MIRROR_CLAMP_TO_EDGE))
                throw std::invalid_argument("Invalid sampler device/filter/address mode");
            target.device = &device;
            target.native_device = device.device;
            const vk::SamplerCreateInfo native{
                .magFilter = static_cast<vk::Filter>(info.sampling),
                .minFilter = static_cast<vk::Filter>(info.sampling),
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = static_cast<vk::SamplerAddressMode>(info.edge),
                .addressModeV = static_cast<vk::SamplerAddressMode>(info.edge),
                .addressModeW = static_cast<vk::SamplerAddressMode>(info.edge),
                .anisotropyEnable = info.sampling != SamplingMode::NEAREST,
                .maxAnisotropy = device.parent_device->device.getProperties().limits.maxSamplerAnisotropy,
                .compareOp = vk::CompareOp::eAlways,
                .borderColor = vk::BorderColor::eFloatTransparentBlack,
            };
            target.sampler = target.native_device.createSampler(native);
        } catch (...) {
            destroy_sampler(target);
            throw;
        }
    }
    void destroy_sampler(impl::SamplerImpl& target) noexcept {
        if (!target.native_device)
            return;
        target.native_device.destroySampler(target.sampler);
        target.sampler = nullptr;
    }
} // namespace VSL_NAMESPACE::operation
