#pragma once
#include "temporary_directory.hpp"
#include "vsl_test_config.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
namespace vsl_test {
    struct CompileResult {
        int status;
        std::string diagnostics;
    };

    inline std::string shell_quote(const std::string& value) {
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
    inline CompileResult compile_probe(const std::string& source) {
        TemporaryDirectory temporary;
        auto input = temporary.path() / "probe.cpp", output = temporary.path() / "diagnostics.txt";
        {
            std::ofstream stream(input);
            stream << source;
        }
#ifdef _MSC_VER
        std::string command = shell_quote(VSL_TEST_CXX) + " /nologo /EHsc /std:c++latest /permissive- /Zc:preprocessor /utf-8 /DNOMINMAX /Zs";
        constexpr auto include_prefix = " /I";
#else
        std::string command = shell_quote(VSL_TEST_CXX) + " -std=c++2c -fsyntax-only -Werror=return-type";
        constexpr auto include_prefix = " -I";
#endif
        for (const auto* include : {VSL_TEST_INCLUDE, VSL_TEST_VULKAN_INCLUDE, VSL_TEST_GLFW_INCLUDE})
            command += include_prefix + shell_quote(include);
        command += " " + shell_quote(input.string()) + " >" + shell_quote(output.string()) + " 2>&1";
#ifdef _WIN32
        command = '"' + command + '"';
#endif
        const int status = std::system(command.c_str());
        std::ifstream stream(output);
        return {status, {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()}};
    }
} // namespace vsl_test
