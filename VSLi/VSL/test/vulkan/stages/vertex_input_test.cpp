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
namespace {
TEST(VertexInputTest, SingleTypeAdditionSelectsAnUnambiguousOverload) {
    pl::VertexInput input;
    const auto result = input.add<float>();
    ASSERT_EQ(input.definitions.size(), 1u);
    ASSERT_EQ(result.definitions.size(), 1u);
    EXPECT_EQ(input.definitions[0].layouts[0].format, vsl::data_format::Float);
    EXPECT_EQ(result.definitions[0].layouts[0].format, vsl::data_format::Float);
    EXPECT_EQ(input.requirements_size(), 4u);
}
TEST(VertexInputTest, TypePacksPreserveArgumentOrderAndPreviouslyAddedShapes) {
    pl::VertexInput input;
    input.add(vsl::data_format::FloatRG);
    input.add<float, std::uint32_t>();
    ASSERT_EQ(input.definitions.size(), 3u);
    EXPECT_EQ(input.definitions[0].layouts[0].format, vsl::data_format::FloatRG);
    EXPECT_EQ(input.definitions[1].layouts[0].format, vsl::data_format::Float);
    EXPECT_EQ(input.definitions[2].layouts[0].format, vsl::data_format::UnsignedInt);
    EXPECT_EQ(input.requirements_size(), 16u);
}
} // namespace

TEST(VertexInputTest, EmptyTypePacksLeaveExistingShapesUnchanged) {
    pl::VertexInput input;
    input.add(vsl::data_format::FloatRG);
    const auto result = input.add<>();
    ASSERT_EQ(input.definitions.size(), 1u);
    ASSERT_EQ(result.definitions.size(), 1u);
    EXPECT_EQ(input.definitions[0].layouts[0].format, vsl::data_format::FloatRG);
    EXPECT_EQ(input.requirements_size(), 8u);
}
