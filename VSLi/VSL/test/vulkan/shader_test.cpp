#include "support/vulkan_fixture.hpp"
#include "support/temporary_directory.hpp"
#include "support/scoped_environment.hpp"

namespace {
    using ShaderVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    static_assert(vsl::get_shader_type<"fixture.vert.spv">() == vsl::ShaderType::VERTEX);
    static_assert(vsl::get_shader_type<"fixture.frag.spv">() == vsl::ShaderType::FRAGMENT);
    static_assert(vsl::get_shader_type<"fixture.comp.spv">() == vsl::ShaderType::COMPUTE);
    static_assert(vsl::get_shader_type<"fixture.unknown">() == vsl::ShaderType::ERROR_);
    TEST(ShaderPathTest, DirectoryNamesDoNotChooseTheShaderStage) {
        EXPECT_EQ(vsl::get_shader_type<"cache.vert.data/lighting.frag.spv">(), vsl::ShaderType::FRAGMENT);
        EXPECT_EQ(vsl::get_shader_type<"cache.frag.data\\simulation.comp.spv">(), vsl::ShaderType::COMPUTE);
        EXPECT_EQ(vsl::get_shader_type<"cache.vert.data/unknown.spv">(), vsl::ShaderType::ERROR_);
    }
    TEST_F(ShaderVulkanIntegrationTest, LoadsVertexAndFragmentModulesWithEntryPointNames) {
        const vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        const vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"),
                                                              "main");
        EXPECT_EQ(vertex.name(), "main");
        EXPECT_EQ(fragment.name(), "main");
        EXPECT_TRUE(vertex.data_->shader_module);
        EXPECT_TRUE(fragment.data_->shader_module);
        EXPECT_EQ(vertex.data_->shader_stage_create_info.stage, vk::ShaderStageFlagBits::eVertex);
        EXPECT_EQ(fragment.data_->shader_stage_create_info.stage, vk::ShaderStageFlagBits::eFragment);
        EXPECT_STREQ(vertex.data_->shader_stage_create_info.pName, vertex.name().c_str());
    }
    TEST_F(ShaderVulkanIntegrationTest, RejectsMissingShaderFiles) {
        EXPECT_THROW((vsl::Shader<vsl::ShaderType::COMPUTE>(device, vsl_test::shader_path("missing.comp.spv"))),
                     vsl::exceptions::RuntimeException);
    }
    TEST(ShaderFlagsTest, BitwiseStageCombinationsPreserveMembership) {
        using namespace vsl;
        const auto stages = ShaderType::VERTEX | ShaderType::FRAGMENT;
        EXPECT_EQ(stages, ShaderFlag::VERTEX | ShaderFlag::FRAGMENT);
        EXPECT_EQ(ShaderType::VERTEX & ShaderType::FRAGMENT, ShaderFlag::ERROR_);
        EXPECT_EQ(ShaderType::VERTEX ^ ShaderType::FRAGMENT, stages);
        EXPECT_EQ(convert(ShaderType::COMPUTE), ShaderFlag::COMPUTE);
    }
} // namespace
namespace {
    TEST_F(ShaderVulkanIntegrationTest, FactoriesInferStageAndHonorExplicitEntryName) {
        const auto vertex = vsl::make_shader<VSL_TEST_SHADER_DIR "/triangle.vert.spv">(device);
        const auto fragment = vsl::make_shader<VSL_TEST_SHADER_DIR "/triangle.frag.spv">(device, "main");
        static_assert(decltype(vertex)::TYPE == vsl::ShaderType::VERTEX);
        static_assert(decltype(fragment)::TYPE == vsl::ShaderType::FRAGMENT);
        EXPECT_EQ(vertex.name(), "main");
        EXPECT_EQ(fragment.name(), "main");
        EXPECT_TRUE(static_cast<bool>(vertex.data_->shader_module));
        EXPECT_TRUE(static_cast<bool>(fragment.data_->shader_module));
    }

    TEST_F(ShaderVulkanIntegrationTest, FactoryLoadsAFragmentShaderInsideADirectoryWithAVertexMarker) {
        vsl_test::TemporaryDirectory temporary;
        const auto directory = temporary.path() / "cache.vert.data";
        std::filesystem::create_directory(directory);
        std::filesystem::copy_file(vsl_test::shader_path("triangle.frag.spv"), directory / "triangle.frag.spv");
        vsl_test::ScopedEnvironment environment("VSL_SHADER_STAGE_TEST_ROOT", temporary.path().string().c_str());
        const auto shader = vsl::make_shader<"${VSL_SHADER_STAGE_TEST_ROOT}/cache.vert.data/triangle.frag.spv">(device);
        EXPECT_EQ(decltype(shader)::TYPE, vsl::ShaderType::FRAGMENT);
        EXPECT_EQ(shader.data_->shader_stage_create_info.stage, vk::ShaderStageFlagBits::eFragment);
        EXPECT_TRUE(static_cast<bool>(shader.data_->shader_module));
    }
} // namespace

namespace {
    TEST_F(ShaderVulkanIntegrationTest, RejectsIncompleteHeadersAndPartialWordsBeforeCreatingAModule) {
        vsl_test::TemporaryDirectory temporary;
        for (const auto byte_count : {0u, 3u, 7u, 4u, 16u, 21u}) {
            const auto path = temporary.path() / (std::to_string(byte_count) + ".vert.spv");
            {
                std::ofstream output(path, std::ios::binary);
                output << std::string(byte_count, '\0');
            }
            EXPECT_THROW((vsl::Shader<vsl::ShaderType::VERTEX>(device, path)), vsl::exceptions::RuntimeException);
            EXPECT_THROW((vsl::Shader<vsl::ShaderType::VERTEX>(device, path, "main")), vsl::exceptions::RuntimeException);
        }
    }
} // namespace
