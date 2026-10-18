#include "support/vulkan_fixture.hpp"

namespace {
using ShaderVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
static_assert(vsl::___Get_ShaderType<"fixture.vert.spv">() == vsl::ShaderType::Vertex);
static_assert(vsl::___Get_ShaderType<"fixture.frag.spv">() == vsl::ShaderType::Fragment);
static_assert(vsl::___Get_ShaderType<"fixture.comp.spv">() == vsl::ShaderType::Compute);
static_assert(vsl::___Get_ShaderType<"fixture.unknown">() == vsl::ShaderType::Error);
TEST_F(ShaderVulkanIntegrationTest, LoadsVertexAndFragmentModulesWithEntryPointNames) {
    const vsl::Shader<vsl::ShaderType::Vertex> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
    const vsl::Shader<vsl::ShaderType::Fragment> fragment(device, vsl_test::shader_path("triangle.frag.spv"),
                                                    "main");
    EXPECT_EQ(vertex.name(), "main");
    EXPECT_EQ(fragment.name(), "main");
    EXPECT_TRUE(vertex._data->shaderModule);
    EXPECT_TRUE(fragment._data->shaderModule);
    EXPECT_EQ(vertex._data->shaderStageCreateInfo.stage, vk::ShaderStageFlagBits::eVertex);
    EXPECT_EQ(fragment._data->shaderStageCreateInfo.stage, vk::ShaderStageFlagBits::eFragment);
    EXPECT_STREQ(vertex._data->shaderStageCreateInfo.pName, vertex.name().c_str());
}
TEST_F(ShaderVulkanIntegrationTest, RejectsMissingShaderFiles) {
    EXPECT_THROW((vsl::Shader<vsl::ShaderType::Compute>(device, vsl_test::shader_path("missing.comp.spv"))),
                 vsl::exceptions::RuntimeException);
}
TEST(ShaderFlagsTest, BitwiseStageCombinationsPreserveMembership) {
    using namespace vsl;
    const auto stages = ShaderType::Vertex | ShaderType::Fragment;
    EXPECT_EQ(stages, ShaderFlag::Vertex | ShaderFlag::Fragment);
    EXPECT_EQ(ShaderType::Vertex & ShaderType::Fragment, ShaderFlag::Error);
    EXPECT_EQ(ShaderType::Vertex ^ ShaderType::Fragment, stages);
    EXPECT_EQ(convert(ShaderType::Compute), ShaderFlag::Compute);
}
} // namespace
namespace {
TEST_F(ShaderVulkanIntegrationTest, FactoriesInferStageAndHonorExplicitEntryName) {
    const auto vertex = vsl::make_shader<VSL_TEST_SHADER_DIR "/triangle.vert.spv">(device);
    const auto fragment = vsl::make_shader<VSL_TEST_SHADER_DIR "/triangle.frag.spv">(device, "main");
    static_assert(decltype(vertex)::Type == vsl::ShaderType::Vertex);
    static_assert(decltype(fragment)::Type == vsl::ShaderType::Fragment);
    EXPECT_EQ(vertex.name(), "main");
    EXPECT_EQ(fragment.name(), "main");
    EXPECT_TRUE(static_cast<bool>(vertex._data->shaderModule));
    EXPECT_TRUE(static_cast<bool>(fragment._data->shaderModule));
}
} // namespace
