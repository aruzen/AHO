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

namespace {
TEST(ShaderCompilerTest, IgnoresDirectoriesWithSourceOrCompiledExtensions) {
    vsl_test::TemporaryDirectory temporary;
    const auto raw = temporary.path() / "raw";
    fs::create_directories(raw / "nested.vert");
    fs::create_directory(temporary.path() / "cached.vert.spv");
    const auto source = raw / "cached.vert";
    write_shader(source);
    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(source, now - std::chrono::hours(2));
    fs::last_write_time(temporary.path() / "cached.vert.spv", now - std::chrono::hours(1));
    vsl::utils::ShaderCompiler compiler(fs::path(VSL_TEST_GLSLC), std::vector<fs::path>{temporary.path()});
    compiler.load();
    EXPECT_TRUE(compiler.compiled.empty());
    EXPECT_EQ(compiler.target, (std::vector<fs::path>{source}));
}
TEST(ShaderCompilerTest, PreservesRegularFilesAndSymlinksToShaderFiles) {
    vsl_test::TemporaryDirectory temporary;
    fs::create_directory(temporary.path() / "raw");
    const auto source = temporary.path() / "raw" / "point.vert";
    const auto output = temporary.path() / "point.vert.spv";
    write_shader(source);
    { std::ofstream stream(output); stream << "cached"; }
    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(source, now - std::chrono::hours(2));
    fs::last_write_time(output, now - std::chrono::hours(1));
    vsl::utils::ShaderCompiler compiler(fs::path(VSL_TEST_GLSLC), std::vector<fs::path>{temporary.path()});
    compiler.load();
    EXPECT_EQ(compiler.compiled, (std::vector<fs::path>{output}));
    EXPECT_TRUE(compiler.target.empty());
    fs::remove(source);
    const auto real = temporary.path() / "real.vert";
    write_shader(real);
    std::error_code error;
    fs::create_symlink(real, source, error);
    if (error)
        GTEST_SKIP() << "Symlink creation unavailable: " << error.message();
    fs::remove(output);
    compiler.load();
    EXPECT_EQ(compiler.target, (std::vector<fs::path>{source}));
    EXPECT_TRUE(compiler.compiled.empty());
}
} // namespace
