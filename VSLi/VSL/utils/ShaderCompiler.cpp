#ifdef _MSC_VER
#include "pch.h"
#endif
#include "ShaderCompiler.h"

#include <map>

VSL_NAMESPACE::utils::ShaderCompiler::ShaderCompiler(std::filesystem::path _glslc_path, std::filesystem::path _shaders_path) : glslc_path(_glslc_path), shaders_path(_shaders_path) {}

namespace fs = std::filesystem;

void VSL_NAMESPACE::utils::ShaderCompiler::load()
{
	if (!fs::exists(shaders_path) || !fs::exists(shaders_path / "raw") || !fs::is_directory(shaders_path / "raw"))
		return;

	std::map<std::string, fs::file_time_type> dates;

	for (const fs::directory_entry& x : fs::directory_iterator(shaders_path)) {
		auto path = x.path();
		if (!path.has_extension() || path.extension() != ".spv")
			continue;
		auto name = path.filename().string();
		dates[name.substr(0, name.size() - 4)] = fs::last_write_time(path);
		compiled.push_back(path);
	}
	for (const fs::directory_entry& x : fs::directory_iterator(shaders_path / "raw")) {
		auto path = x.path();
		if (!path.has_extension() || !(path.extension() == ".vert" || path.extension() == ".frag"))
			continue;
		if (dates.contains(path.filename().string()) && fs::last_write_time(path) < dates[path.filename().string()])
			continue;
		target.push_back(path);
	}
}

void VSL_NAMESPACE::utils::ShaderCompiler::compile()
{
	if (!fs::exists(glslc_path) || !glslc_path.has_filename())
		return;
	for (auto p : target) {
		auto src_file = fs::absolute(p);
		auto&& src = src_file.string();
		auto&& dst = (src_file.parent_path().parent_path() / (src_file.filename().string() + ".spv")).string();
		system((glslc_path.string() + " \"" + src + "\" -o \"" + dst + "\"").c_str());
	}
}

