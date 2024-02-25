#pragma once
#include "../define.h"
#include "pv.h"

#include "../dimention.h"
#include "swapchain.h"

namespace vsl {
	template<typename D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT>
	struct View {
		View(vsl::Swapchain device);

		size_t size();

		std::shared_ptr<_impl::View_impl> _data;
	};
}