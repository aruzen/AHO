#pragma once
#include "../define.h"
#include "pv.h"

#include "swapchain.h"
#include "../utils/VSLArray.h"
#include "pipeline_layout.h"

namespace VSL_NAMESPACE {
	template<is_rectangle T = __VSLD2RectangleDefault<int>>
	struct Viewport : __VSLD2Rectangle<T> {
		using element_type = T::element_type;

		// std::shared_ptr<_impl::Viewport_impl> _data;

		Viewport();
		Viewport(T* t);
		Viewport(const element_type& x, const element_type& y, const element_type& width, const element_type& height);
		Viewport(SwapchainAccessor swapchain);
	
		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}