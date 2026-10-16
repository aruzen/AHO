#pragma once
#include "temporary_directory.hpp"
#include "vsl_test_config.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
namespace vsl_test {
inline std::string shell_quote(const std::string &value) {
#ifdef _WIN32
    std::string result = "\"";
    for (char c : value) {
        if (c == '"')
            result += '\\';
        result += c;
    }
    return result + '"';
#else
    std::string result = "'";
    for (char c : value)
        result += c == '\'' ? "'\\''" : std::string(1, c);
    return result + "'";
#endif
}
struct CompileResult {
    int status;
    std::string diagnostics;
};
inline CompileResult compile_probe(const std::string &source) {
    TemporaryDirectory temporary;
    auto input = temporary.path() / "probe.cpp", output = temporary.path() / "diagnostics.txt";
    {
        std::ofstream stream(input);
        stream << source;
    }
    std::string command = shell_quote(VSL_TEST_CXX) + " -std=c++23 -fsyntax-only -Werror=return-type";
    for (const auto *include : {VSL_TEST_INCLUDE, VSL_TEST_VULKAN_INCLUDE, VSL_TEST_GLFW_INCLUDE})
        command += " -I" + shell_quote(include);
    command += " " + shell_quote(input.string()) + " >" + shell_quote(output.string()) + " 2>&1";
    const int status = std::system(command.c_str());
    std::ifstream stream(output);
    return {status, {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()}};
}
} // namespace vsl_test
