#pragma once
#include "../../define.h"
#include "../pv.h"

namespace VSL_NAMESPACE::pipeline_layout {
	struct DepthStencil {
		void injection(struct VSL_NAMESPACE::_impl::CreateInfo& info);
	};
}