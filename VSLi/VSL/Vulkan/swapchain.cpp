#include "pch.h"
#include "../define.h"

#include "surface.h"
#include "swapchain.h"

#include "_pimpls.h"

using namespace vsl::_impl::helper;

vsl::Swapchain::Swapchain(vsl::LogicalDeviceAccessor device, std::optional<std::shared_ptr<Surface>> _surface)
{
	_data = std::shared_ptr<_impl::Swapchain_impl>(new _impl::Swapchain_impl);
	_data->device = device._data;

	std::shared_ptr<_impl::Surface_impl> surface = _surface ? _surface.value()->_data : device._data->parentSurface;;
	std::shared_ptr<_impl::PhysicalDevice_impl> pdevice = device._data->parentDevice;

	vk::SwapchainCreateInfoKHR createInfo{};
	createInfo.surface = surface->surface;
	createInfo.minImageCount = device._data->imageCount;
	createInfo.imageFormat = device._data->surfaceFormat.format;
	createInfo.imageColorSpace = device._data->surfaceFormat.colorSpace;
	createInfo.imageExtent = device._data->extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

	QueueFamilyIndices indices = findQueueFamilies(pdevice->device, surface->surface);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
		createInfo.preTransform = device._data->preTransform;
	}

	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = device._data->presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	_data->swapChain = device._data->device.createSwapchainKHR(createInfo);

	// swapchain image
	_data->swapChainImages = device._data->device.getSwapchainImagesKHR(_data->swapChain);
	_data->swapChainImageFormat = device._data->surfaceFormat.format;
	_data->swapChainExtent = device._data->extent;
}

VSL_NAMESPACE::_impl::Swapchain_impl::~Swapchain_impl()
{
	device->device.destroySwapchainKHR(swapChain);
}