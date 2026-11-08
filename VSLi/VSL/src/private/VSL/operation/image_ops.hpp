#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/image_create_info.hpp>

namespace VSL_NAMESPACE::operation {
    struct ImageViewNativeCalls {
        void* context = nullptr;
        VkResult (*create)(void*, VkDevice, const VkImageViewCreateInfo*, VkImageView*) = nullptr;
    };
    // deviceと外部imageの寿命は呼出側が保証する。native資源の所有だけを区別する。
    void initialize_image(impl::ImageImpl& target, impl::LogicalDeviceImpl& device, const ImageCreateInfo& info);
    void initialize_borrowed_image(impl::ImageImpl& target, impl::LogicalDeviceImpl& device,
                                   const ImageCreateInfo& info, vk::Image image, const ImageViewNativeCalls& calls = {});
    void destroy_image(impl::ImageImpl& target) noexcept;
} // namespace VSL_NAMESPACE::operation
