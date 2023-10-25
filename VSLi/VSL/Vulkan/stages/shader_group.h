#pragma once
#include "../../define.h"
#include "../pv.h"

#include <initializer_list>

#include "../shader.h"

namespace VSL_NAMESPACE::pipeline_layout {
	
	struct ShaderGroup {
		std::shared_ptr<VSL_NAMESPACE::_impl::ShaderGroup_impl> _data;

		ShaderGroup();

		void injection(struct VSL_NAMESPACE::_impl::CreateInfo& info);
	};
}