#include "support/vulkan_fixture.hpp"
#include <VSL/utils/typed_safe_resource.hpp>

namespace {
using First =
    vsl::typed::ResourceBindingPoint<2, vsl::graphic_resource::Type::UniformBuffer, vsl::ShaderFlag::Vertex>;
using Second = vsl::typed::ResourceBindingPoint<5, vsl::graphic_resource::Type::StorageBuffer,
                                                vsl::ShaderFlag::Compute, 3>;
using Points = vsl::typed::ResourceBindingPoints<First, Second>;
static_assert(Points::size() == 2 && Points::get<1>().binding == 5);
TEST(TypedResourceTest, BindingDefinitionsRetainIndexTypeStageAndArrayCount) {
    const auto point = Points::get<1>();
    EXPECT_EQ(point.binding, 5u);
    EXPECT_EQ(point.bindingType, vsl::graphic_resource::Type::StorageBuffer);
    EXPECT_EQ(point.shaderType, vsl::ShaderFlag::Compute);
    EXPECT_EQ(point.amount, 3u);
}
TEST(TypedResourceTest, VertexAndPushDefinitionsRetainExplicitLayout) {
    using Attribute = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRG, 3, 8>;
    using Shape = vsl::typed::VertexShapeDefinition<
        4, 16, vsl::pipeline_layout::VertexInputShapeDefinition::UpdateTiming::NextInstance, Attribute>;
    EXPECT_EQ(Shape::size(), 1u);
    EXPECT_EQ(Shape::get<0>().format, vsl::data_format::FloatRG);
    EXPECT_EQ(Shape::get<0>().location, 3u);
    EXPECT_EQ(Shape::get<0>().offset, 8u);
    using Push = vsl::typed::PushContentDefinitionTag<vsl::ShaderFlag::Fragment, 16, 32>;
    using Definitions = vsl::typed::PushContentDefinitions<Push>;
    EXPECT_EQ(Definitions::size(), 1u);
    EXPECT_EQ(Definitions::get<0>().targetShader, vsl::ShaderFlag::Fragment);
    EXPECT_EQ(Definitions::get<0>().offset, 32u);
    EXPECT_EQ(Definitions::get<0>().size, 16u);
    EXPECT_EQ(vsl::typed::ResourceBindingPoints<>::size(), 0u);
    EXPECT_EQ(vsl::typed::PushContentDefinitions<>::size(), 0u);
}
using TypedResourceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(TypedResourceVulkanIntegrationTest, MakesBindingLayoutFromTypedPoints) {
    const auto layout = Points{}.make(device);
    const auto actual = layout.getBindingPoints();
    ASSERT_EQ(actual.size(), 2u);
    EXPECT_EQ(actual[0], First::value);
    EXPECT_EQ(actual[1], Second::value);
}
} // namespace
namespace {
TEST_F(TypedResourceVulkanIntegrationTest, DISABLED_TypedVertexInputInjectsItsDeclaredAttribute) {
    using Attribute = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRG, 3, 8>;
    using Shape = vsl::typed::VertexShapeDefinition<
        4, 16, vsl::pipeline_layout::VertexInputShapeDefinition::UpdateTiming::NextInstance, Attribute>;
    auto layout = make_layout_state();
    vsl::typed::TypedVertexInput<Shape>{}.injection(layout);
    const auto &state = layout._data->info->vertexInput;
    ASSERT_EQ(state.vertexBindingDescriptionCount, 1u);
    ASSERT_EQ(state.vertexAttributeDescriptionCount, 1u);
    EXPECT_EQ(state.pVertexBindingDescriptions[0].binding, 4u);
    EXPECT_EQ(state.pVertexBindingDescriptions[0].stride, 16u);
    EXPECT_EQ(state.pVertexBindingDescriptions[0].inputRate, vk::VertexInputRate::eInstance);
    EXPECT_EQ(state.pVertexAttributeDescriptions[0].location, 3u);
    EXPECT_EQ(state.pVertexAttributeDescriptions[0].offset, 8u);
}
} // namespace
