#include <VSL/define.hpp>

#include <VSL/vulkan/surface.hpp>
#include <VSL/vulkan/swapchain.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/command.hpp>
#include "swapchain_sharing.hpp"

#undef max

using namespace VSL_NAMESPACE::_impl::helper;

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities{};
    std::vector<vk::SurfaceFormatKHR> formats{};
    std::vector<vk::PresentModeKHR> presentModes{};
};

SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);
    return details;
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

VSL_NAMESPACE::Swapchain::Swapchain(VSL_NAMESPACE::LogicalDeviceAccessor device, std::shared_ptr<Surface> surface, std::optional<VSL_NAMESPACE::SwapchainAccessor> old,
                                    std::optional<int> width, std::optional<int> height, ImageType usage) {
    _data = std::shared_ptr<_impl::Swapchain_impl>(new _impl::Swapchain_impl);
    _data->device = device._data;
    _data->surface = surface->_data;

    std::shared_ptr<_impl::PhysicalDevice_impl> pdevice = device._data->parentDevice;

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pdevice->device, surface->_data->surface);
    auto swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

    if (!swapChainAdequate)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Swapchain", "PhysicalDevices do not have requirements.");

    _data->surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    _data->presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);

    if (not width.has_value() || not height.has_value())
        glfwGetFramebufferSize((GLFWwindow*)surface->_data->window,
                               width ? nullptr : &width.emplace(0),
                               height ? nullptr : &height.emplace(0));
    _data->extent = chooseSwapExtent(swapChainSupport.capabilities, width.value(), height.value());

    _data->imageCount = std::max(swapChainSupport.capabilities.minImageCount, DEFAULT_BUFFER_SIZE);
    _data->preTransform = swapChainSupport.capabilities.currentTransform;

    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        _data->imageCount > swapChainSupport.capabilities.maxImageCount) {
        _data->imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = surface->_data->surface;
    createInfo.minImageCount = _data->imageCount;
    createInfo.imageFormat = _data->surfaceFormat.format;
    createInfo.imageColorSpace = _data->surfaceFormat.colorSpace;
    createInfo.imageExtent = _data->extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlags(static_cast<std::uint32_t>(usage));
    if (!contain(usage, ImageType::ColorAttachment) ||
        (swapChainSupport.capabilities.supportedUsageFlags & createInfo.imageUsage) != createInfo.imageUsage)
        throw std::invalid_argument("Surface does not support requested swapchain image usage");

    QueueFamilyIndices indices = findQueueFamilies(pdevice->device, surface->_data->surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    configureSwapchainSharing(createInfo, queueFamilyIndices, _data->preTransform);

    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = _data->presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = old.has_value() ? old->_data->swapChain : VK_NULL_HANDLE;

    vk::SwapchainKHR created;
    if (auto result = device._data->device.createSwapchainKHR(&createInfo, nullptr, &created); result != vk::Result::eSuccess)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Swapchain", "vkCreateSwapchainKHR failed: " + vk::to_string(result));
    _data->swapChain = created;

    _data->swapChainImages = device._data->device.getSwapchainImagesKHR(_data->swapChain);
    _data->swapChainImageFormat = _data->surfaceFormat.format;
    _data->swapChainExtent = _data->extent;

    images.resize(_data->swapChainImages.size());
    for (size_t i = 0; i < _data->swapChainImages.size(); i++) {
        vk::ImageViewCreateInfo createInfo;
        createInfo.image = _data->swapChainImages[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = _data->swapChainImageFormat;
        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        auto imageData = std::make_shared<_impl::Image_impl>();
        imageData->device = device._data;
        imageData->swapchain = _data;
        imageData->usage = usage;
        imageData->format = _data->swapChainImageFormat;
        imageData->count = 1;
        imageData->width = _data->swapChainExtent.width;
        imageData->height = _data->swapChainExtent.height;
        imageData->view = _data->device->device.createImageView(createInfo);
        imageData->image = _data->swapChainImages[i];
        images[i]._data = imageData;
    }
}

size_t VSL_NAMESPACE::SwapchainAccessor::getSwapImageSize() {
    return images.size();
}

VSL_NAMESPACE::_impl::Swapchain_impl::~Swapchain_impl() {
    device->device.destroySwapchainKHR(swapChain);
}

VSL_NAMESPACE::data_format::___Format VSL_NAMESPACE::SwapchainAccessor::getFormat() const {
    const auto type = static_cast<data_format::___FormatTypes>(_data->swapChainImageFormat);
    const auto format = data_format::convert(type);
    return format.type() == data_format::___FormatTypes::Undefined ? data_format::___Format(type, 0) : format;
}
