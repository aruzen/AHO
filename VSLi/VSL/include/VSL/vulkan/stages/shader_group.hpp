#pragma once
#include "../../define.hpp"
#include "../pv.hpp"

#include <initializer_list>

#include "../shader.hpp"
#include "../pipeline_layout.hpp"

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