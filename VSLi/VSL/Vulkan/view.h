#pragma once
#include "../define.h"
#include "pv.h"

#include "../dimention.h"
#include "swapchain.h"

namespace vsl {
	template<typename D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT, bool V = vsl::validation>
	struct View {
		View(vsl::Swapchain<V> device);

		std::shared_ptr<_impl::View_impl> _data;
	};
}