#ifdef _MSC_VER
#include "pch.h"
#endif
#include <VSL/utils/shader_compiler.hpp>
#include <VSL/utils/string.hpp>

#include <map>

VSL_NAMESPACE::utils::ShaderCompiler::ShaderCompiler(std::filesystem::path glslc_path, std::vector<std::filesystem::path> shader_dirs) : glslc_path(glslc_path), shader_dirs(shader_dirs) {}

vsl::utils::ShaderCompiler::ShaderCompiler(std::string glslc_str, std::initializer_list<std::string> shader_str) {
    glslc_path = std::filesystem::path(vsl::expand_environments(glslc_str));
    shader_dirs.reserve(shader_str.size());
    for (auto shader_dir : shader_str)
        shader_dirs.push_back(std::filesystem::path(vsl::expand_environments(shader_dir)));
}

namespace fs = std::filesystem;

void VSL_NAMESPACE::utils::ShaderCompiler::load()
{
    std::map<std::string, fs::file_time_type> dates;
    for (auto shaders_path : shader_dirs) {
        dates.clear();
        if (!fs::exists(shaders_path) || !fs::exists(shaders_path / "raw") || !fs::is_directory(shaders_path / "raw"))
            continue;

        for (const fs::directory_entry &x: fs::directory_iterator(shaders_path)) {
            auto path = x.path();
            if (!path.has_extension() || path.extension() != ".spv")
                continue;
            auto name = path.filename().string();
            dates[name.substr(0, name.size() - 4)] = fs::last_write_time(path);
            compiled.push_back(path);
        }

        for (const fs::directory_entry &x: fs::directory_iterator(shaders_path / "raw")) {
            auto path = x.path();
            if (!path.has_extension() ||
                !(path.extension() == ".vert" || path.extension() == ".frag" || path.extension() == ".comp"))
                continue;
            if (dates.contains(path.filename().string()) && fs::last_write_time(path) < dates[path.filename().string()])
                continue;
            target.push_back(path);
        }
    }
}

void VSL_NAMESPACE::utils::ShaderCompiler::compile()
{
	// if (!fs::exists(glslc_path) || !glslc_path.has_filename())
	//	return;
	for (auto p : target) {
		auto src_file = fs::absolute(p);
		auto&& src = src_file.string();
		auto&& dst = (src_file.parent_path().parent_path() / (src_file.filename().string() + ".spv")).string();
		system((glslc_path.string() + " \"" + src + "\" -o \"" + dst + "\"").c_str());
	}
}
