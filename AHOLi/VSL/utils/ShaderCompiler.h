#pragma once
#include "../define.h"

#include <filesystem>
#include <vector>

namespace VSL_NAMESPACE::utils {
	class ShaderCompiler {
		std::filesystem::path glslc_path, shaders_path;
	public:
		ShaderCompiler(std::filesystem::path _glslc_path, std::filesystem::path _shaders_path);

		void load();
		void compile();

		std::vector<std::filesystem::path> compiled, target;
	};
}