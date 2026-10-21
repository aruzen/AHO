#include "support/vulkan_fixture.hpp"
#include "support/temporary_directory.hpp"

namespace {
    using ShaderVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    static_assert(vsl::get_shader_type<"fixture.vert.spv">() == vsl::ShaderType::VERTEX);
    static_assert(vsl::get_shader_type<"fixture.frag.spv">() == vsl::ShaderType::FRAGMENT);
    static_assert(vsl::get_shader_type<"fixture.comp.spv">() == vsl::ShaderType::COMPUTE);
    static_assert(vsl::get_shader_type<"fixture.unknown">() == vsl::ShaderType::ERROR_);
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
