#pragma once
#include "../define.hpp"

#include <filesystem>
#include <vector>

namespace VSL_NAMESPACE::utils {
	class ShaderCompiler {
		std::filesystem::path glslc_path;
        std::vector<std::filesystem::path> shader_dirs;
	public:
        ShaderCompiler(std::filesystem::path glslc_path, std::vector<std::filesystem::path> shader_dirs);
		ShaderCompiler(std::string glslc_path, std::initializer_list<std::string> shader_dirs);

		void load();
		void compile();

		std::vector<std::filesystem::path> compiled, target;
	};
}