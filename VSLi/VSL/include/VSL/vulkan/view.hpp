#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include "../dimension.hpp"
#include "swapchain.hpp"

namespace vsl {
	template<typename D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT>
	struct View {
		View(vsl::Swapchain device);

		size_t size();

		std::shared_ptr<_impl::View_impl> _data;
	};
}