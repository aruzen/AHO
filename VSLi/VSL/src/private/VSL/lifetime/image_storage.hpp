#pragma once
#include <VSL/operation/image_ops.hpp>
#include <VSL/lifetime/resource_use.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct ImageSlotStorage {
        alignas(impl::ImageImpl) std::byte image[sizeof(impl::ImageImpl)];
        alignas(ResourceUseState) std::byte use[sizeof(ResourceUseState)];
    };
    // 未構築かつsizeof/alignof(ImageSlotStorage)を満たす領域を渡す。deviceは呼出側が保持する。
    void construct_image(void* storage, LogicalDeviceAccessor device, const ImageCreateInfo& info, vk::Image borrowed = {});
    // 使用完了／取消の確認後、構築済みslotへ一度だけ呼ぶ。
    void destroy_image(void* storage) noexcept;
    impl::ImageImpl& image_at(void* storage);
    std::shared_ptr<impl::ImageImpl> make_heap_image(LogicalDeviceAccessor device, const ImageCreateInfo& info);
    std::shared_ptr<impl::ImageImpl> make_swapchain_image(LogicalDeviceAccessor device, const ImageCreateInfo& info,
                                                          std::shared_ptr<impl::SwapchainImpl> swapchain, vk::Image image);
} // namespace VSL_NAMESPACE::lifetime
