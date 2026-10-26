#include <AHO/x/resource/shader.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <fstream>

namespace {
    using AHO_NAMESPACE::x::resource::load_spirv;
    class ShaderAssetTest : public ::testing::Test {
    protected:
        std::filesystem::path dir, file;
        void SetUp() override;
        void TearDown() override {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
        void write(std::vector<std::uint32_t> words);
    };

    void ShaderAssetTest::SetUp() {
        static std::atomic<unsigned> ID{};
        dir = std::filesystem::temp_directory_path() / ("aho-spirv-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(ID++));
        std::filesystem::create_directory(dir);
        file = dir / "shader.spv";
    }

    void ShaderAssetTest::write(std::vector<std::uint32_t> words) {
        std::ofstream out(file, std::ios::binary);
        for (auto word : words)
            for (int i = 0; i < 4; ++i)
                out.put(char((word >> (8 * i)) & 255));
    }
    TEST_F(ShaderAssetTest, LoadsOwnedLittleEndianWords) {
        std::vector<std::uint32_t> words{0x07230203, 0x00010000, 0, 1, 0, 0x00010000};
        write(words);
        auto loaded = load_spirv(file);
        std::filesystem::remove(file);
        EXPECT_EQ(loaded, words);
    }
    TEST_F(ShaderAssetTest, RejectsMissingTruncatedAndUnalignedFiles) {
        EXPECT_THROW(load_spirv(file), std::runtime_error);
        write({0x07230203});
        EXPECT_THROW(load_spirv(file), std::runtime_error);
        write({0x07230203, 0x00010000, 0, 1, 0});
        std::ofstream(file, std::ios::binary | std::ios::app).put(0);
        EXPECT_THROW(load_spirv(file), std::runtime_error);
    }
    TEST_F(ShaderAssetTest, RejectsInvalidHeadersAndInstructionLengths) {
        const std::vector<std::uint32_t> valid{0x07230203, 0x00010000, 0, 1, 0, 0x00010000};
        for (auto [index, value] : {std::pair{0, 0u}, std::pair{1, 0x00020000u}, std::pair{3, 0u}, std::pair{4, 1u}, std::pair{5, 0u}, std::pair{5, 0x00020000u}}) {
            auto words = valid;
            words[index] = value;
            write(words);
            EXPECT_THROW(load_spirv(file), std::runtime_error);
        }
    }

    TEST_F(ShaderAssetTest, LoadsARealCheckedInShaderModule) {
        auto path = std::filesystem::path(__FILE__).parent_path() / "../../../built-in-resource/shaders/fc.frag.spv";
        auto words = load_spirv(path);
        ASSERT_GT(words.size(), 5);
        EXPECT_EQ(words[0], 0x07230203u);
    }
} // namespace
