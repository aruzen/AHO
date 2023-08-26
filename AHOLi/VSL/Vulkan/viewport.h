#pragma once
#include "../define.h"
#include "pv.h"

#include "swapchain.h"
#include "../utils/VSLArray.h"

namespace VSL_NAMESPACE {
	template<is_rectangle T>
	struct Viewport : __VSLD2Rectangle<T> {
		using element_type = T::element_type;

		Viewport();
		Viewport(T* t);
		Viewport(const element_type& x, const element_type& y, const element_type& width, const element_type& height);
	};
}