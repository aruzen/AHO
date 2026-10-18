#include "support/vulkan_fixture.hpp"
#include <VSL/utils/spir_reflector.hpp>

namespace {
TEST(SpirvReflectorTest, ReflectsBindingsVertexInputsAndPushConstantMembersFromFile) {
    vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection.vert.spv"));
    const auto &result = reflector.generated;
    ASSERT_EQ(result.binding_points.size(), 1u);
    const auto &bindings = result.binding_points.at(0);
    ASSERT_EQ(bindings.size(), 1u);
    EXPECT_EQ(bindings[0].binding, 0u);
    EXPECT_EQ(bindings[0].bindingType, vsl::graphic_resource::Type::UniformBuffer);
    EXPECT_EQ(bindings[0].shaderType, vsl::ShaderFlag::Vertex);
    EXPECT_EQ(bindings[0].amount, 1u);
    ASSERT_TRUE(result.vertex_input);
    ASSERT_EQ(result.vertex_input->definitions.size(), 2u);
    EXPECT_EQ(result.vertex_input->definitions[0].layouts[0].format, vsl::data_format::FloatRG);
    EXPECT_EQ(result.vertex_input->definitions[1].layouts[0].format, vsl::data_format::FloatRGB);
    ASSERT_TRUE(result.push_constants);
    ASSERT_EQ(result.push_constants->definitions.size(), 1u);
    EXPECT_EQ(result.push_constants->definitions[0].offset, 0u);
    EXPECT_EQ(result.push_constants->definitions[0].size, 16u);
    EXPECT_EQ(result.push_constants->definitions[0].targetShader, vsl::ShaderFlag::Vertex);
}
TEST(SpirvReflectorTest, BinaryConstructorRecognizesComputeResourcesAndStage) {
    const auto bytes = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
    vsl::utils::SPIRVReflector reflector({}, bytes);
    const auto &binding = reflector.generated.binding_points.at(0).at(0);
    EXPECT_EQ(binding.bindingType, vsl::graphic_resource::Type::StorageBuffer);
    EXPECT_EQ(binding.shaderType, vsl::ShaderFlag::Compute);
    ASSERT_TRUE(reflector.generated.push_constants);
    EXPECT_EQ(reflector.generated.push_constants->definitions[0].size, 4u);
}
TEST(SpirvReflectorTest, MissingFileReportsPathLoadingFailure) {
    EXPECT_THROW(vsl::utils::SPIRVReflector({}, vsl_test::shader_path("absent.spv")),
                 vsl::exceptions::RuntimeException);
}
using SpirvReflectorVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(SpirvReflectorVulkanIntegrationTest, GeneratedBindingLayoutsRetainDefinitionsAndRepeatCount) {
    vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("fill.comp.spv"));
    const auto layouts = reflector.generated.makeBindingLayout(2);
    ASSERT_EQ(layouts.size(), 3u);
    for (const auto &layout : layouts) {
        const auto points = layout.getBindingPoints();
        ASSERT_EQ(points.size(), 1u);
        EXPECT_EQ(points[0], reflector.generated.binding_points.at(0).at(0));
    }
}
} // namespace

namespace {
TEST(SpirvReflectorTest, DescriptorArraysUseTheirElementCountsForEverySupportedResourceCategory) {
    vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("descriptor_arrays.comp.spv"));
    const auto &points = reflector.generated.binding_points.at(0);
    ASSERT_EQ(points.size(), 4u);
    for (const auto &[binding, type, amount] : {
             std::tuple{0u, vsl::graphic_resource::Type::UniformBuffer, 2u},
             std::tuple{1u, vsl::graphic_resource::Type::StorageBuffer, 3u},
             std::tuple{2u, vsl::graphic_resource::Type::StorageImage, 4u},
             std::tuple{3u, vsl::graphic_resource::Type::CombinedImageSampler, 5u}}) {
        const auto found = std::ranges::find(points, binding, &vsl::graphic_resource::BindingPoint::binding);
        ASSERT_NE(found, points.end());
        EXPECT_EQ(found->bindingType, type);
        EXPECT_EQ(found->amount, amount);
    }
}
TEST(SpirvReflectorTest, MultidimensionalDescriptorArrayCountsMultiplyAllExtents) {
    vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("descriptor_multidimensional.comp.spv"));
    const auto &points = reflector.generated.binding_points.at(0);
    const auto found = std::ranges::find(points, 0u, &vsl::graphic_resource::BindingPoint::binding);
    ASSERT_NE(found, points.end());
    EXPECT_EQ(found->amount, 6u);
}
TEST(SpirvReflectorTest, UnresolvedAndOverflowingDescriptorCountsReportTheirCause) {
    for (const auto &[file, reason] : {
             std::pair{"descriptor_runtime.comp.spv", "runtime"},
             std::pair{"descriptor_count_overflow.comp.spv", "overflow"}}) {
        try {
            vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path(file));
            FAIL() << file << " cannot be represented as a fixed uint32 descriptor count";
        } catch (const vsl::exceptions::RuntimeException &error) {
            EXPECT_NE(error.reason.find(reason), std::string::npos) << error.reason;
        }
    }
}
TEST_F(SpirvReflectorVulkanIntegrationTest, FixedArrayCountsSurviveGeneratedNativeBindingLayouts) {
    vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("descriptor_arrays.comp.spv"));
    auto layouts = reflector.generated.makeBindingLayout();
    ASSERT_EQ(layouts.size(), 1u);
    const auto points = layouts.front().getBindingPoints();
    ASSERT_EQ(points.size(), 4u);
    for (const auto &point : points) EXPECT_EQ(point.amount, point.binding + 2u);
}
} // namespace

namespace {
TEST(SpirvReflectorTest, SpecializationDescriptorCountsUseDefaultsAndEvaluateDefaultExpressions) {
    for (const auto &[file, amount] : {std::pair{"descriptor_specialization.comp.spv", 3u},
                                     std::pair{"descriptor_specialization_expression.comp.spv", 7u}}) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path(file));
        const auto &points = reflector.generated.binding_points.at(0);
        const auto found = std::ranges::find(points, 0u, &vsl::graphic_resource::BindingPoint::binding);
        ASSERT_NE(found, points.end());
        EXPECT_EQ(found->amount, amount);
    }
}
} // namespace
