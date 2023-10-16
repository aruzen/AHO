#pragma once
#include "../../define.h"
#include "../pv.h"

#include <initializer_list>

namespace VSL_NAMESPACE::pipeline_layout {
	
	struct ShaderGroup {
		std::shared_ptr<VSL_NAMESPACE::_impl::pipeline_layout::ShaderGroup_impl> _data;

		ShaderGroup(std::initializer_list<>);

		void injection(struct VSL_NAMESPACE::_impl::CreateInfo& info);
	};
}