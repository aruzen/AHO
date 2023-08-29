#pragma once
#include "../../define.h"
#include "../pv.h"

namespace VSL_NAMESPACE::pipeline_layout {
	struct ColorBlend {
		void injection(struct VSL_NAMESPACE::_impl::CreateInfo& info);
	};
}