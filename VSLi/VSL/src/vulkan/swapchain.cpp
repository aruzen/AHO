#include <VSL/define.hpp>
#include <VSL/lifetime/device_owner.hpp>

#include <VSL/vulkan/surface.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#include <VSL/vulkan/swapchain.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include "swapchain_sharing.hpp"

#undef max

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities{};
    std::vector<vk::SurfaceFormatKHR> formats{};
    std::vector<vk::PresentModeKHR> present_modes{};
};

SwapChainSupportDetails query_swap_chain_support(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.present_modes = device.getSurfacePresentModesKHR(surface);
    return details;
}

vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    for (const auto& available_format : available_formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available_format;
        }
    }
    return available_formats[0];
}

vk::PresentModeKHR choose_swap_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == vk::PresentModeKHR::eMailbox) {
            return available_present_mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        vk::Extent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height,
                                          capabilities.maxImageExtent.height);

        return actual_extent;
    }
}

VSL_NAMESPACE::Swapchain::Swapchain(VSL_NAMESPACE::LogicalDeviceAccessor device, std::shared_ptr<Surface> surface, std::optional<VSL_NAMESPACE::SwapchainAccessor> old,
                                    std::optional<int> width, std::optional<int> height, ImageType usage) {
    if (!surface)
        throw std::invalid_argument("Swapchain requires a Surface owner");
    const auto source = surface->data_;
    (void)Surface(source).owner_ref();
    lifetime::require_device_owner(device);
    if (!device.data_->swapchain_enabled || !device.data_->parent_device ||
        device.data_->parent_device->vulkan != source->vulkan)
        throw std::invalid_argument("Swapchain requires a Device on its Surface Instance");
    lifetime::GLFWCallScope scope;
    if (old && (!old->data_ || !old->data_->swap_chain || old->data_->device != device.data_ ||
                old->data_->surface != source))
        throw std::invalid_argument("Old swapchain must be valid and use the same device and surface");
    data_ = std::shared_ptr<impl::SwapchainImpl>(new impl::SwapchainImpl);
    data_->device = device.data_;
    data_->surface = source;

    auto* pdevice = device.data_->parent_device;
    if (!pdevice->device.getSurfaceSupportKHR(device.data_->present_family, source->surface))
        throw std::invalid_argument("Device presentation queue does not support this Surface");

    SwapChainSupportDetails swap_chain_support = query_swap_chain_support(pdevice->device, source->surface);
    auto swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();

    if (!swap_chain_adequate)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Swapchain", "PhysicalDevices do not have requirements.");

    data_->surface_format = choose_swap_surface_format(swap_chain_support.formats);
    data_->present_mode = choose_swap_present_mode(swap_chain_support.present_modes);

    if (not width.has_value() || not height.has_value())
        glfwGetFramebufferSize((GLFWwindow*)source->window,
                               width ? nullptr : &width.emplace(0),
                               height ? nullptr : &height.emplace(0));
    data_->extent = choose_swap_extent(swap_chain_support.capabilities, width.value(), height.value());

    data_->image_count = std::max(swap_chain_support.capabilities.minImageCount, DEFAULT_BUFFER_SIZE);
    data_->pre_transform = swap_chain_support.capabilities.currentTransform;

    if (swap_chain_support.capabilities.maxImageCount > 0 &&
        data_->image_count > swap_chain_support.capabilities.maxImageCount) {
        data_->image_count = swap_chain_support.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info{};
    create_info.surface = source->surface;
    create_info.minImageCount = data_->image_count;
    create_info.imageFormat = data_->surface_format.format;
    create_info.imageColorSpace = data_->surface_format.colorSpace;
    create_info.imageExtent = data_->extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = vk::ImageUsageFlags(static_cast<std::uint32_t>(usage));
    if (!contain(usage, ImageType::COLOR_ATTACHMENT) ||
        (swap_chain_support.capabilities.supportedUsageFlags & create_info.imageUsage) != create_info.imageUsage)
        throw std::invalid_argument("Surface does not support requested swapchain image usage");

    uint32_t queue_family_indices[] = {device.data_->graphics_family, device.data_->present_family};

    impl::helper::configure_swapchain_sharing(create_info, queue_family_indices, data_->pre_transform);

    create_info.compositeAlpha = impl::helper::choose_swapchain_composite_alpha(swap_chain_support.capabilities.supportedCompositeAlpha);
    create_info.presentMode = data_->present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = old.has_value() ? old->data_->swap_chain : VK_NULL_HANDLE;

    vk::SwapchainKHR created;
    if (auto result = device.data_->device.createSwapchainKHR(&create_info, nullptr, &created); result != vk::Result::eSuccess)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Swapchain", "vkCreateSwapchainKHR failed: " + vk::to_string(result));
    data_->swap_chain = created;

    data_->swap_chain_images = device.data_->device.getSwapchainImagesKHR(data_->swap_chain);
    data_->swap_chain_image_format = data_->surface_format.format;
    data_->swap_chain_extent = data_->extent;

    images.resize(data_->swap_chain_images.size());
    for (size_t i = 0; i < data_->swap_chain_images.size(); i++) {
        const ImageCreateInfo info{usage, data_->swap_chain_extent.width, data_->swap_chain_extent.height, get_format()};
        images[i].data_ = lifetime::make_swapchain_image(device, info, data_, data_->swap_chain_images[i]);
    }
}

size_t VSL_NAMESPACE::SwapchainAccessor::get_swap_image_size() {
    return images.size();
}

VSL_NAMESPACE::impl::SwapchainImpl::~SwapchainImpl() {
    device->device.destroySwapchainKHR(swap_chain);
}

VSL_NAMESPACE::data_format::Format VSL_NAMESPACE::SwapchainAccessor::get_format() const {
    const auto type = static_cast<data_format::FormatTypes>(data_->swap_chain_image_format);
    const auto format = data_format::convert(type);
    return format.type() == data_format::FormatTypes::UNDEFINED ? data_format::Format(type, 0) : format;
}
