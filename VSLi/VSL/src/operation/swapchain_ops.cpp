#include <VSL/operation/swapchain_ops.hpp>
#include <VSL/exceptions.hpp>
#include "../vulkan/swapchain_sharing.hpp"
#include <algorithm>
#include <utility>

namespace VSL_NAMESPACE::operation {
    namespace {
        vk::SurfaceFormatKHR select_format(const std::vector<vk::SurfaceFormatKHR>& formats);
        vk::PresentModeKHR select_mode(const std::vector<vk::PresentModeKHR>& modes);
        std::vector<vk::Image> enumerate_images(PendingSwapchain& pending);
        vk::SurfaceFormatKHR select_format(const std::vector<vk::SurfaceFormatKHR>& formats) {
            for (const auto& format : formats)
                if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                    return format;
            return formats.front();
        }
        vk::PresentModeKHR select_mode(const std::vector<vk::PresentModeKHR>& modes) {
            return std::ranges::find(modes, vk::PresentModeKHR::eMailbox) != modes.end() ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
        }
        std::vector<vk::Image> enumerate_images(PendingSwapchain& pending) {
            const auto call = [&](std::uint32_t* count, VkImage* images) {
                return pending.calls.images ? pending.calls.images(pending.calls.context, pending.native_device, pending.chain, count, images)
                                            : vkGetSwapchainImagesKHR(pending.native_device, pending.chain, count, images);
            };
            for (;;) {
                std::uint32_t count = 0;
                auto result = call(&count, nullptr);
                if (result != VK_SUCCESS)
                    vk::detail::throwResultException(static_cast<vk::Result>(result), "Swapchain image count failed");
                if (!count)
                    throw std::runtime_error("Swapchain returned no images");
                std::vector<VkImage> native(count);
                result = call(&count, native.data());
                if (result == VK_INCOMPLETE)
                    continue;
                if (result != VK_SUCCESS)
                    vk::detail::throwResultException(static_cast<vk::Result>(result), "Swapchain image enumeration failed");
                if (!count || count > native.size())
                    throw std::runtime_error("Swapchain returned an invalid image count");
                std::vector<vk::Image> images;
                images.reserve(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    if (!native[i])
                        throw std::runtime_error("Swapchain returned an empty image");
                    images.emplace_back(native[i]);
                }
                return images;
            }
        }
    } // namespace
    PendingSwapchain::PendingSwapchain(PendingSwapchain&& other) noexcept
        : device(other.device), surface(other.surface), native_device(other.native_device), chain(std::exchange(other.chain, {})),
          format(other.format), mode(other.mode), extent(other.extent), transform(other.transform), usage(other.usage),
          present_fences(other.present_fences), images(std::move(other.images)), calls(other.calls) {}
    PendingSwapchain::~PendingSwapchain() {
        if (chain) {
            if (calls.destroy)
                calls.destroy(calls.context, native_device, chain);
            else
                vkDestroySwapchainKHR(native_device, chain, nullptr);
        }
    }
    PendingSwapchain prepare_swapchain(impl::LogicalDeviceImpl& device, impl::SurfaceImpl& surface, const SwapchainCreateInfo& info,
                                       impl::SwapchainImpl* old, const SwapchainNativeCalls& calls) {
        if (!device.device || !device.swapchain_enabled || !device.parent_device || !surface.surface || device.parent_device->vulkan != surface.vulkan)
            throw std::invalid_argument("Swapchain requires a Device on its Surface Instance");
        if (info.present_fences != PresentFencePolicy::LEGACY && info.present_fences != PresentFencePolicy::REQUIRE_FENCE)
            throw std::invalid_argument("Unknown presentation fence policy");
        if (info.present_fences == PresentFencePolicy::REQUIRE_FENCE && !has_device_features(device.enabled_features, DeviceFeature::SWAPCHAIN_MAINTENANCE1))
            throw std::invalid_argument("Swapchain requires enabled maintenance1 for present fences");
        if (old) {
            require_active_swapchain(*old);
            if (old->device != &device || old->surface != &surface)
                throw std::invalid_argument("Old swapchain must use the same Device and Surface");
        }
        if (!info.width || !info.height || !*info.width || !*info.height)
            throw std::invalid_argument("Swapchain requires a nonzero resolved extent");
        const auto physical = device.parent_device->device;
        if (!physical.getSurfaceSupportKHR(device.present_family, surface.surface))
            throw std::invalid_argument("Device presentation queue does not support this Surface");
        const auto capabilities = physical.getSurfaceCapabilitiesKHR(surface.surface);
        const auto formats = physical.getSurfaceFormatsKHR(surface.surface);
        const auto modes = physical.getSurfacePresentModesKHR(surface.surface);
        if (formats.empty() || modes.empty())
            throw exceptions::RuntimeException("Swapchain", "PhysicalDevice does not support a swapchain");
        PendingSwapchain pending;
        pending.device = &device;
        pending.surface = &surface;
        pending.native_device = device.device;
        pending.format = select_format(formats);
        pending.mode = select_mode(modes);
        pending.transform = capabilities.currentTransform;
        pending.usage = info.usage;
        pending.present_fences = info.present_fences;
        pending.calls = calls;
        pending.extent = capabilities.currentExtent;
        if (pending.extent.width == std::numeric_limits<std::uint32_t>::max())
            pending.extent = vk::Extent2D{std::clamp(*info.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                                          std::clamp(*info.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
        if (!pending.extent.width || !pending.extent.height)
            throw std::runtime_error("Swapchain framebuffer extent is zero");
        auto count = std::max(capabilities.minImageCount, DEFAULT_BUFFER_SIZE);
        if (capabilities.maxImageCount)
            count = std::min(count, capabilities.maxImageCount);
        vk::SwapchainCreateInfoKHR native{};
        native.surface = surface.surface;
        native.minImageCount = count;
        native.imageFormat = pending.format.format;
        native.imageColorSpace = pending.format.colorSpace;
        native.imageExtent = pending.extent;
        native.imageArrayLayers = 1;
        native.imageUsage = vk::ImageUsageFlags(static_cast<std::uint32_t>(info.usage));
        if (!contain(info.usage, ImageType::COLOR_ATTACHMENT) || (capabilities.supportedUsageFlags & native.imageUsage) != native.imageUsage)
            throw std::invalid_argument("Surface does not support requested swapchain image usage");
        const std::uint32_t families[]{device.graphics_family, device.present_family};
        impl::helper::configure_swapchain_sharing(native, families, pending.transform);
        native.compositeAlpha = impl::helper::choose_swapchain_composite_alpha(capabilities.supportedCompositeAlpha);
        native.presentMode = pending.mode;
        native.clipped = VK_TRUE;
        native.oldSwapchain = old ? old->swap_chain : vk::SwapchainKHR{};
        VkSwapchainKHR created = VK_NULL_HANDLE;
        // native呼出し後は成功／失敗によらず旧世代を現役へ戻せない。
        if (old) {
            require_active_swapchain(*old);
            old->state = SwapchainState::RETIRED;
        }
        const auto result = calls.create ? calls.create(calls.context, device.device, &static_cast<const VkSwapchainCreateInfoKHR&>(native), &created)
                                         : vkCreateSwapchainKHR(device.device, &static_cast<const VkSwapchainCreateInfoKHR&>(native), nullptr, &created);
        if (result != VK_SUCCESS)
            throw exceptions::RuntimeException("Swapchain", "vkCreateSwapchainKHR failed: " + vk::to_string(static_cast<vk::Result>(result)));
        if (!created)
            throw std::runtime_error("Swapchain creation returned an empty handle");
        pending.chain = created;
        pending.images = enumerate_images(pending);
        return pending;
    }
    void adopt_swapchain(impl::SwapchainImpl& target, PendingSwapchain& pending) noexcept {
        target.device = pending.device;
        target.surface = pending.surface;
        target.native_device = pending.native_device;
        target.surface_format = pending.format;
        target.present_mode = pending.mode;
        target.extent = target.swap_chain_extent = pending.extent;
        target.pre_transform = pending.transform;
        target.image_count = static_cast<std::uint32_t>(pending.images.size());
        target.swap_chain = std::exchange(pending.chain, {});
        target.swap_chain_images = std::move(pending.images);
        target.swap_chain_image_format = target.surface_format.format;
        target.present_fences = pending.present_fences;
        target.state = SwapchainState::ACTIVE;
    }
    void destroy_swapchain(impl::SwapchainImpl& target) noexcept {
        const auto chain = std::exchange(target.swap_chain, {});
        if (chain && target.native_device)
            vkDestroySwapchainKHR(target.native_device, chain, nullptr);
        target.state = SwapchainState::UNUSABLE;
    }
    void require_active_swapchain(const impl::SwapchainImpl& target) {
        if (!target.swap_chain || target.state != SwapchainState::ACTIVE)
            throw std::logic_error("Swapchain generation is not active");
    }
} // namespace VSL_NAMESPACE::operation

VSL_NAMESPACE::impl::SwapchainImpl::~SwapchainImpl() {
    operation::destroy_swapchain(*this);
}
