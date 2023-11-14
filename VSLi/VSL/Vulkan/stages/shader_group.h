#pragma once
#include "../../define.h"
#include "../pv.h"

#include <initializer_list>

#include "../shader.h"

namespace VSL_NAMESPACE::pipeline_layout {
	struct ShaderGroup {
		std::shared_ptr<_impl::pipeline_layout::ShaderGroup_impl> _data;

		ShaderGroup(std::initializer_list<ShaderAccessor> shaders, std::string name);

		void addShader(ShaderAccessor shader);

		void removeShader(std::string name);
		void removeShader(ShaderAccessor shader);

		void injection(struct _impl::CreateInfo& info);
	};
}