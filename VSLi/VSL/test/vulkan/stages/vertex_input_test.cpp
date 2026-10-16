#include "support/vulkan_fixture.hpp"

namespace {
namespace pl = vsl::pipeline_layout;
TEST(VertexInputTest, AddsFormatsShapesAndExplicitDefinitionsWithExpectedSizes) {
    pl::VertexInput input;
    EXPECT_EQ(input.requirements_size(), 0u);
    input.add(vsl::data_format::FloatRG);
    input.add({vsl::data_format::FloatRGB, vsl::data_format::UnsignedInt});
    EXPECT_EQ(input.definitions.size(), 3u);
    EXPECT_EQ(input.requirements_size(), 24u);
    input.add_shape({vsl::data_format::FloatRG, vsl::data_format::FloatRGB});
    ASSERT_EQ(input.definitions.size(), 4u);
    EXPECT_EQ(input.definitions.back().layouts.size(), 2u);
    EXPECT_EQ(input.requirements_size(), 44u);
    input.add(pl::VertexInputShapeDefinition{.layouts = {{vsl::data_format::Float}}, .size = 16});
    EXPECT_EQ(input.requirements_size(), 60u);
}
TEST(VertexInputTest, InitializesAndPrependsShapeDefinitions) {
    const pl::VertexInputShapeDefinition first{.layouts = {{vsl::data_format::FloatRG}}, .binding = 2};
    const pl::VertexInputShapeDefinition second{.layouts = {{vsl::data_format::FloatRGB}}, .binding = 5};
    pl::VertexInput input({first});
    input.add(std::initializer_list<pl::VertexInputShapeDefinition>{second});
    ASSERT_EQ(input.definitions.size(), 2u);
    EXPECT_EQ(input.definitions[0].binding, 5u);
    EXPECT_EQ(input.definitions[1].binding, 2u);
}
using VertexInputVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(VertexInputVulkanIntegrationTest, InjectsExplicitAndAutomaticLocationsStridesAndOffsets) {
    pl::VertexInput input({{.layouts = {{vsl::data_format::FloatRG, 3, 4}, {vsl::data_format::FloatRGB}},
                            .binding = 2,
                            .size = 32,
                            .updateTiming = pl::VertexInputShapeDefinition::UpdateTiming::NextInstance}});
    auto layout = make_layout_state();
    input.injection(layout);
    const auto &info = layout._data->info->vertexInput;
    ASSERT_EQ(info.vertexBindingDescriptionCount, 1u);
    ASSERT_EQ(info.vertexAttributeDescriptionCount, 2u);
    EXPECT_EQ(info.pVertexBindingDescriptions[0].binding, 2u);
    EXPECT_EQ(info.pVertexBindingDescriptions[0].stride, 32u);
    EXPECT_EQ(info.pVertexBindingDescriptions[0].inputRate, vk::VertexInputRate::eInstance);
    EXPECT_EQ(info.pVertexAttributeDescriptions[0].location, 3u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[0].offset, 4u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[1].location, 4u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[1].offset, 12u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[1].format, vk::Format::eR32G32B32Sfloat);
}
} // namespace
#include "support/compile_probe.hpp"
namespace {
TEST(VertexInputCompilationTest, DISABLED_SingleTypeAdditionSelectsAnUnambiguousOverload) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
void probe(){vsl::pipeline_layout::VertexInput input;input.add<float>();}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
} // namespace
