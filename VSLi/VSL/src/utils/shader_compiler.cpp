#include <VSL/utils/shader_compiler.hpp>
#include <VSL/utils/string.hpp>
#include <VSL/exceptions.hpp>

#include <cerrno>
#ifdef _WIN32
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <crt_externs.h>
#else
extern char** environ;
#endif
#endif
#include <map>

VSL_NAMESPACE::utils::ShaderCompiler::ShaderCompiler(std::filesystem::path glslc_path, std::vector<std::filesystem::path> shader_dirs) : _glslc_path(glslc_path), _shader_dirs(shader_dirs) {}

VSL_NAMESPACE::utils::ShaderCompiler::ShaderCompiler(std::string glslc_str, std::initializer_list<std::string> shader_str) {
    _glslc_path = std::filesystem::path(VSL_NAMESPACE::expand_environments(glslc_str));
    _shader_dirs.reserve(shader_str.size());
    for (auto shader_dir : shader_str)
        _shader_dirs.push_back(std::filesystem::path(VSL_NAMESPACE::expand_environments(shader_dir)));
}

namespace fs = std::filesystem;

namespace {
#ifdef _WIN32
    std::wstring quote_process_argument(const std::wstring& argument) {
        std::wstring result(1, L'"');
        std::size_t backslashes = 0;
        for (const wchar_t character : argument) {
            if (character == L'\\') {
                ++backslashes;
                continue;
            }
            result.append(backslashes * (character == L'"' ? 2 : 1), L'\\');
            backslashes = 0;
            if (character == L'"')
                result += L'\\';
            result += character;
        }
        result.append(backslashes * 2, L'\\');
        result += L'"';
        return result;
    }
#endif

    int run_compiler(const fs::path& executable, const fs::path& source, const fs::path& destination) {
#ifdef _WIN32
        const auto program = quote_process_argument(executable.native());
        const auto input = quote_process_argument(source.native());
        const auto output = quote_process_argument(destination.native());
        const wchar_t* arguments[] = {program.c_str(), input.c_str(), L"-o", output.c_str(), nullptr};
        const auto status = _wspawnvp(_P_WAIT, executable.c_str(), arguments);
        return status == -1 ? (errno ? -errno : -1) : static_cast<int>(status);
#else
        auto program = executable.native();
        auto input = source.native();
        auto output = destination.native();
        char option[] = "-o";
        char* arguments[] = {program.data(), input.data(), option, output.data(), nullptr};
        pid_t child;
#ifdef __APPLE__
        char** environment = *_NSGetEnviron();
#else
        char** environment = environ;
#endif
        const int error = posix_spawnp(&child, program.c_str(), nullptr, nullptr, arguments, environment);
        if (error != 0)
            return -error;
        int status = 0;
        while (waitpid(child, &status, 0) == -1)
            if (errno != EINTR)
                return -errno;
        return status;
#endif
    }

    std::string path_text(const fs::path& path) {
        const auto utf8 = path.u8string();
        return {utf8.begin(), utf8.end()};
    }
} // namespace

void VSL_NAMESPACE::utils::ShaderCompiler::load() {
    compiled.clear();
    target.clear();
    std::map<std::string, fs::file_time_type> dates;
    for (auto shaders_path : _shader_dirs) {
        dates.clear();
        if (!fs::exists(shaders_path) || !fs::exists(shaders_path / "raw") || !fs::is_directory(shaders_path / "raw"))
            continue;

        for (const fs::directory_entry& x : fs::directory_iterator(shaders_path)) {
            if (!x.is_regular_file())
                continue;
            auto path = x.path();
            if (!path.has_extension() || path.extension() != ".spv")
                continue;
            auto name = path.filename().string();
            dates[name.substr(0, name.size() - 4)] = fs::last_write_time(path);
            compiled.push_back(path);
        }

        for (const fs::directory_entry& x : fs::directory_iterator(shaders_path / "raw")) {
            if (!x.is_regular_file())
                continue;
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

void VSL_NAMESPACE::utils::ShaderCompiler::compile() {
    for (const auto& path : target) {
        const auto source = fs::absolute(path);
        auto destination = source.parent_path().parent_path() / source.filename();
        destination += ".spv";
        const int status = run_compiler(_glslc_path, source, destination);
        if (status != 0)
            throw exceptions::RuntimeException("ShaderCompiler", "Failed to compile " + path_text(source) +
                                                                     " using " + path_text(_glslc_path) + " (status " + std::to_string(status) + ")");
    }
}
