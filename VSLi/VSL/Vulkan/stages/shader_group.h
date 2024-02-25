#pragma once
#include "../../define.h"
#include "../pv.h"

#include <initializer_list>

#include "../shader.h"
#include "../pipeline_layout.h"

namespace VSL_NAMESPACE::pipeline_layout {
	struct ShaderGroup {
		std::shared_ptr<_impl::pipeline_layout::ShaderGroup_impl> _data;

		ShaderGroup(std::string name, std::initializer_list<ShaderAccessor> shaders);

		void addShader(ShaderAccessor shader);

		void removeShader(std::string name);
		void removeShader(ShaderAccessor shader);

		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};
}