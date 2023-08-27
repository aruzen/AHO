#include "viewport.h"

#include "_pimpls.h"

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Viewport<T>::Viewport() {
	// _data = std::shared_ptr<vsl::_impl::Viewport_impl>(new vsl::_impl::Viewport_impl);
}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Viewport<T>::Viewport(T* t) : __VSLD2Rectangle<T>(t) {
	// _data = std::shared_ptr<vsl::_impl::Viewport_impl>(new vsl::_impl::Viewport_impl);
}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Viewport<T>::Viewport(const element_type& x, const element_type& y, const element_type& width, const element_type& height) {
	// _data = std::shared_ptr<vsl::_impl::Viewport_impl>(new vsl::_impl::Viewport_impl);

	this->x = x;
	this->y = y;
	this->width = width;
	this->height = height
}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Viewport<T>::Viewport(SwapchainAcsessor swapchain) {
	// _data = std::shared_ptr<vsl::_impl::Viewport_impl>(new vsl::_impl::Viewport_impl);
	
	this->x = 0;
	this->y = 0;
	this->height = swapchain._data->swapChainExtent.height;
	this->width = swapchain._data->swapChainExtent.width;
}

template<VSL_NAMESPACE::is_rectangle T>
void VSL_NAMESPACE::Viewport<T>::injection(VSL_NAMESPACE::_impl::CreateInfo& info) {
	vk::Viewport viewport;
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	info.viewports.push_back(viewport);
}
