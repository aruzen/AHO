#include "pch.h"
#include "../define.h"

#include "view.h"

#include "_pimpls.h"
#include "_dimention.h"

template<typename D, bool V>
VSL_NAMESPACE::View<D, V>::View(vsl::Swapchain<V> swapchain)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::View_impl>(new VSL_NAMESPACE::_impl::View_impl);
	_data->swapChainImageViews.resize(swapchain._data->swapChainImages.size());
	_data->device = swapchain._data->device;

	for (size_t i = 0; i < _data->swapChainImageViews.size(); i++) {
		vk::ImageViewCreateInfo createInfo{};
		createInfo.image = swapchain._data->swapChainImages[i];
		constexpr auto viewType = vsl::helper::dimention::toImageViewType<D>();
		createInfo.viewType = viewType;
		createInfo.format = swapchain._data->swapChainImageFormat;
		createInfo.components.r = vk::ComponentSwizzle::eIdentity;
		createInfo.components.g = vk::ComponentSwizzle::eIdentity;
		createInfo.components.b = vk::ComponentSwizzle::eIdentity;
		createInfo.components.a = vk::ComponentSwizzle::eIdentity;
		createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		_data->swapChainImageViews[i] = _data->device->device.createImageView(createInfo);
	}
}

VSL_NAMESPACE::_impl::View_impl::~View_impl()
{
	for (auto&& imageView : swapChainImageViews) {
		device->device.destroyImageView(imageView);
	}
}

template struct vsl::View<>;