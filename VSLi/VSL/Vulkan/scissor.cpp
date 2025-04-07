#ifdef _MSC_VER
#include "pch.h"
#endif
#include "scissor.h"

#include "_pimpls.h"

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Scissor<T>::Scissor() {
	// _data = std::shared_ptr<vsl::_impl::Scissor_impl>(new vsl::_impl::Scissor_impl);
}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Scissor<T>::Scissor(T* t) : __VSLD2Rectangle<T>(t) {
	// _data = std::shared_ptr<vsl::_impl::Scissor_impl>(new vsl::_impl::Scissor_impl);
}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Scissor<T>::Scissor(const element_type& x, const element_type& y, const element_type& width, const element_type& height) {
	// _data = std::shared_ptr<vsl::_impl::Scissor_impl>(new vsl::_impl::Scissor_impl);

	this->x = x;
	this->y = y;
	this->width = width;
	this->height = height;
}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Scissor<T>::Scissor(SwapchainAccessor swapchain) {
	// _data = std::shared_ptr<vsl::_impl::Scissor_impl>(new vsl::_impl::Scissor_impl);
	
	this->x = 0;
	this->y = 0;
	this->height = swapchain._data->swapChainExtent.height;
	this->width = swapchain._data->swapChainExtent.width;
}
