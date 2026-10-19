#include <gtest/gtest.h>
#include <VSL/utils/shader_compiler.hpp>
#include "support/temporary_directory.hpp"
#include "vsl_test_config.hpp"
#include <fstream>
#include <array>
#include <algorithm>
namespace {
namespace fs = std::filesystem;
void write_shader(const fs::path &path) {
    std::ofstream stream(path);
    stream << "#version 450\nvoid main(){gl_Position=vec4(0,0,0,1);}\n";
}
TEST(ShaderCompilerTest, LoadsSupportedExtensionsAndCompilesValidSpirv) {
    vsl_test::TemporaryDirectory temporary;
    const auto directory = temporary.path() / "shaders with spaces";
    fs::create_directories(directory / "raw");
    write_shader(directory / "raw" / "point.vert");
    {
        std::ofstream ignored(directory / "raw" / "notes.txt");
        ignored << "ignored";
    }
    vsl::utils::ShaderCompiler compiler(fs::path(VSL_TEST_GLSLC), std::vector<fs::path>{directory});
    compiler.load();
    ASSERT_EQ(compiler.target.size(), 1u);
    EXPECT_EQ(compiler.target[0], directory / "raw" / "point.vert");
    EXPECT_TRUE(compiler.compiled.empty());
    compiler.compile();
    std::ifstream stream(directory / "point.vert.spv", std::ios::binary);
    ASSERT_TRUE(stream.good());
    std::uint32_t magic = 0;
    stream.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0x07230203u);
}
TEST(ShaderCompilerTest, StringConstructorSkipsMissingDirectoriesAndNewerCompiledOutputs) {
    vsl_test::TemporaryDirectory temporary;
    fs::create_directory(temporary.path() / "raw");
    auto source = temporary.path() / "raw" / "cached.vert";
    auto output = temporary.path() / "cached.vert.spv";
    write_shader(source);
    {
        std::ofstream stream(output);
        stream << "cached";
    }
    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(source, now - std::chrono::hours(2));
    fs::last_write_time(output, now - std::chrono::hours(1));
    vsl::utils::ShaderCompiler compiler(std::string(VSL_TEST_GLSLC),
                                        {temporary.path().string(), (temporary.path() / "missing").string()});
    compiler.load();
    EXPECT_TRUE(compiler.target.empty());
    EXPECT_EQ(compiler.compiled, (std::vector<fs::path>{output}));
}
TEST(ShaderCompilerTest, ReloadDoesNotDuplicateTargets) {
    vsl_test::TemporaryDirectory temporary;
    fs::create_directory(temporary.path() / "raw");
    write_shader(temporary.path() / "raw" / "point.vert");
    vsl::utils::ShaderCompiler compiler(fs::path(VSL_TEST_GLSLC), std::vector<fs::path>{temporary.path()});
    compiler.load();
    compiler.load();
    EXPECT_EQ(compiler.target.size(), 1u);
}
} // namespace

#include <VSL/exceptions.hpp>
namespace {
TEST(ShaderCompilerTest, InvalidShaderReportsTheSourceAndStopsCompilation) {
    vsl_test::TemporaryDirectory temporary;
    fs::create_directory(temporary.path() / "raw");
    const auto invalid = temporary.path() / "raw" / "broken.vert";
    const auto valid = temporary.path() / "raw" / "later.vert";
    { std::ofstream output(invalid); output << "this is not valid GLSL"; }
    write_shader(valid);
    vsl::utils::ShaderCompiler compiler(fs::path(VSL_TEST_GLSLC), std::vector<fs::path>{});
    compiler.target = {invalid, valid};
    try {
        compiler.compile();
        FAIL() << "Compilation failure was silently ignored";
    } catch (const vsl::exceptions::RuntimeException &error) {
        EXPECT_NE(std::string(error.what()).find(invalid.string()), std::string::npos);
    }
    EXPECT_FALSE(fs::exists(temporary.path() / "later.vert.spv"));
    // A failed batch must not prevent a later, explicitly requested retry.
    compiler.target = {valid};
    EXPECT_NO_THROW(compiler.compile());
    std::ifstream stream(temporary.path() / "later.vert.spv", std::ios::binary);
    ASSERT_TRUE(stream.good());
    std::uint32_t magic = 0;
    stream.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0x07230203u);
}
TEST(ShaderCompilerTest, MissingExecutableReportsCompilationFailure) {
    vsl_test::TemporaryDirectory temporary;
    fs::create_directory(temporary.path() / "raw");
    const auto source = temporary.path() / "raw" / "point.vert";
    write_shader(source);
    const auto missing = temporary.path() / "missing-glslc";
    vsl::utils::ShaderCompiler compiler(missing, std::vector<fs::path>{});
    compiler.target = {source};
    try {
        compiler.compile();
        FAIL() << "Missing executable was silently ignored";
    } catch (const vsl::exceptions::RuntimeException &error) {
        const std::string message(error.what());
        EXPECT_NE(message.find(source.string()), std::string::npos);
        EXPECT_NE(message.find(missing.string()), std::string::npos);
    }
}
} // namespace

TEST(ShaderCompilerTest, EmptyBatchDoesNotInvokeAMissingExecutable) {
    vsl_test::TemporaryDirectory temporary;
    vsl::utils::ShaderCompiler compiler(temporary.path() / "missing-glslc", std::vector<fs::path>{});
    EXPECT_NO_THROW(compiler.compile());
}

namespace {
void expect_spirv(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    ASSERT_TRUE(stream.good()) << path;
    std::uint32_t magic = 0;
    stream.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0x07230203u);
}
TEST(ShaderCompilerTest, RunsCompilerExecutableWhosePathContainsSpaces) {
    vsl_test::TemporaryDirectory temporary;
    const auto executable = temporary.path() / (std::string("compiler with spaces") + fs::path(VSL_TEST_GLSLC).extension().string());
    fs::copy_file(VSL_TEST_GLSLC, executable);
    fs::permissions(executable, fs::status(VSL_TEST_GLSLC).permissions());
    fs::create_directory(temporary.path() / "raw");
    const auto source = temporary.path() / "raw" / "point.vert";
    write_shader(source);
    vsl::utils::ShaderCompiler compiler(executable, std::vector<fs::path>{});
    compiler.target = {source};
    ASSERT_NO_THROW(compiler.compile());
    expect_spirv(temporary.path() / "point.vert.spv");
}
TEST(ShaderCompilerTest, TreatsShellMetacharactersInSourceAndOutputPathsLiterally) {
    vsl_test::TemporaryDirectory temporary;
    const auto directory = temporary.path() / "literal $HOME & (shader)' directory";
    fs::create_directories(directory / "raw");
    const auto source = directory / "raw" / "point $HOME & (literal)'.vert";
    write_shader(source);
    vsl::utils::ShaderCompiler compiler(fs::path(VSL_TEST_GLSLC), std::vector<fs::path>{});
    compiler.target = {source};
    ASSERT_NO_THROW(compiler.compile());
    expect_spirv(directory / (source.filename().string() + ".spv"));
}
} // namespace

#include "support/scoped_environment.hpp"
TEST(ShaderCompilerTest, FindsCompilerOnPathWithoutShellExpansion) {
    vsl_test::TemporaryDirectory temporary;
    const auto binary_directory = temporary.path() / "compiler bin";
    fs::create_directory(binary_directory);
    const auto executable = binary_directory / fs::path(VSL_TEST_GLSLC).filename();
    fs::copy_file(VSL_TEST_GLSLC, executable);
    fs::permissions(executable, fs::status(VSL_TEST_GLSLC).permissions());
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    const auto* previous = std::getenv("PATH");
    const auto search_path = binary_directory.string() + separator + (previous ? previous : "");
    vsl_test::ScopedEnvironment environment("PATH", search_path.c_str());
    fs::create_directory(temporary.path() / "raw");
    const auto source = temporary.path() / "raw" / "point.vert";
    write_shader(source);
    vsl::utils::ShaderCompiler compiler(executable.filename(), std::vector<fs::path>{});
    compiler.target = {source};
    ASSERT_NO_THROW(compiler.compile());
    expect_spirv(temporary.path() / "point.vert.spv");
}

namespace {
TEST(ShaderCompilerTest, ReloadRefreshesCompiledFilesAndChangedSourceLists) {
    vsl_test::TemporaryDirectory temporary;
    const auto raw = temporary.path() / "raw";
    fs::create_directory(raw);
    const auto cached = raw / "cached.vert";
    const auto pending = raw / "pending.frag";
    const auto binary = temporary.path() / "cached.vert.spv";
    write_shader(cached);
    write_shader(pending);
    { std::ofstream output(binary); output << "cached"; }
    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(cached, now - std::chrono::hours(2));
    fs::last_write_time(binary, now - std::chrono::hours(1));
    vsl::utils::ShaderCompiler compiler(fs::path(VSL_TEST_GLSLC), std::vector<fs::path>{temporary.path()});
    compiler.load();
    compiler.load();
    EXPECT_EQ(compiler.compiled, (std::vector<fs::path>{binary}));
    EXPECT_EQ(compiler.target, (std::vector<fs::path>{pending}));
    fs::remove(pending);
    fs::remove(binary);
    const auto added = raw / "added.comp";
    write_shader(added);
    compiler.load();
    EXPECT_TRUE(compiler.compiled.empty());
    auto actual = compiler.target;
    auto expected = std::vector<fs::path>{cached, added};
    std::ranges::sort(actual);
    std::ranges::sort(expected);
    EXPECT_EQ(actual, expected);
    fs::remove_all(raw);
    compiler.load();
    EXPECT_TRUE(compiler.compiled.empty());
    EXPECT_TRUE(compiler.target.empty());
}
} // namespace
