#ifdef _MSC_VER
#include "pch.h"
#endif

#include "../define.h"

#include "surface.h"
#include "swapchain.h"
#include "../exceptions.h"

#include "_pimpls.h"

using namespace vsl::_impl::helper;

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

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat: availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb
            && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode: availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

vsl::Swapchain::Swapchain(vsl::LogicalDeviceAccessor device, std::shared_ptr<Surface> surface,
                          std::optional<int> width, std::optional<int> height) {
    _data = std::shared_ptr<_impl::Swapchain_impl>(new _impl::Swapchain_impl);
    _data->device = device._data;

    std::shared_ptr<_impl::PhysicalDevice_impl> pdevice = device._data->parentDevice;

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pdevice->device, surface->_data->surface);
    auto swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

    if (!swapChainAdequate)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Swapchain", "PhysicalDevices do not have requirements.");

    _data->surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    _data->presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);

    if (not width.has_value() || not height.has_value())
        glfwGetFramebufferSize((GLFWwindow *) surface->_data->window,
                               width ? nullptr : &width.emplace(0),
                               height ? nullptr : &height.emplace(0));
    _data->extent = chooseSwapExtent(swapChainSupport.capabilities, width.value(), height.value());

    _data->imageCount = swapChainSupport.capabilities.minImageCount + 1;
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
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = findQueueFamilies(pdevice->device, surface->_data->surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
        createInfo.preTransform = _data->preTransform;
    }

    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = _data->presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    _data->swapChain = device._data->device.createSwapchainKHR(createInfo);

    // swapchain image
    _data->swapChainImages = device._data->device.getSwapchainImagesKHR(_data->swapChain);
    _data->swapChainImageFormat = _data->surfaceFormat.format;
    _data->swapChainExtent = _data->extent;
}

size_t VSL_NAMESPACE::SwapchainAccessor::getSwapImageSize() {
    return _data->imageCount;
}

VSL_NAMESPACE::_impl::Swapchain_impl::~Swapchain_impl() {
    device->device.destroySwapchainKHR(swapChain);
}
