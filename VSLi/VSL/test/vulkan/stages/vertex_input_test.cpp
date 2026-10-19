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

#include <limits>
namespace {
TEST(VertexInputTest, InferredRequirementsIncludeExplicitGapsAndTheLargestAttributeEnd) {
    pl::VertexInput input({{.layouts = {{vsl::data_format::FloatRG, 0, 24},
                                      {vsl::data_format::Float, 1, 0},
                                      {vsl::data_format::Float, 2}}}});
    EXPECT_EQ(input.requirements_size(), 32u);
    input.add(pl::VertexInputShapeDefinition{.layouts = {{vsl::data_format::Float, 3, 12}}});
    EXPECT_EQ(input.requirements_size(), 48u);
}
using VertexStrideVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(VertexStrideVulkanIntegrationTest, NativeStridesUseTheLargestEndAndPreserveExplicitSize) {
    pl::VertexInput input({{.layouts = {{vsl::data_format::FloatRG, 0, 24},
                                      {vsl::data_format::Float, 1, 0}, {vsl::data_format::Float, 2}}},
                           {.layouts = {{vsl::data_format::Float, 3, 4}}, .size = 20}});
    auto layout = make_layout_state();
    input.injection(layout);
    const auto &info = layout._data->info->vertexInput;
    ASSERT_EQ(info.vertexBindingDescriptionCount, 2u);
    EXPECT_EQ(info.pVertexBindingDescriptions[0].stride, 32u);
    EXPECT_EQ(info.pVertexBindingDescriptions[1].stride, 20u);
    EXPECT_EQ(input.requirements_size(), 52u);
    ASSERT_EQ(info.vertexAttributeDescriptionCount, 4u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[0].offset, 24u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[1].offset, 0u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[2].offset, 4u);
    EXPECT_EQ(info.pVertexAttributeDescriptions[3].offset, 4u);
}
TEST_F(VertexStrideVulkanIntegrationTest, AttributeExtentOverflowIsRejectedInsteadOfWrapping) {
    pl::VertexInput input({{.layouts = {{vsl::data_format::FloatRG, 0,
                                       std::numeric_limits<std::uint32_t>::max() - 2}}}});
    EXPECT_THROW(input.requirements_size(), vsl::exceptions::RuntimeException);
    auto layout = make_layout_state();
    EXPECT_THROW(input.injection(layout), vsl::exceptions::RuntimeException);
}
} // namespace
#include "support/render_fixture.hpp"
namespace {
using VertexStrideRenderVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
TEST_F(VertexStrideRenderVulkanIntegrationTest, PaddedInterleavedVerticesProduceTheExpectedGpuColor) {
    struct Vertex { float padding; float position[2]; float color[3]; };
    const std::array<Vertex, 3> vertices{{{0, {-0.75f, -0.75f}, {0.25f, 0.5f, 0.75f}},
                                         {0, {0.75f, -0.75f}, {0.25f, 0.5f, 0.75f}},
                                         {0, {0, 0.75f}, {0.25f, 0.5f, 0.75f}}}};
    static_assert(sizeof(Vertex) == 24);
    vsl::StagingBuffer<vsl::MemoryType::VertexBuffer> buffer(device, vertices);
    pipeline.reset();
    layout.reset();
    vertex.emplace(device, vsl_test::shader_path("vertices.vert.spv"));
    pl::VertexInput input({{.layouts = {{vsl::data_format::FloatRG, 0, 4},
                                      {vsl::data_format::FloatRGB, 1}}}});
    EXPECT_EQ(input.requirements_size(), sizeof(Vertex));
    layout.emplace(device, pl::ShaderGroup("padded vertices", {*vertex, *fragment}), input, pl::ColorBlend{},
                   pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None}, pl::Multisample{},
                   pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
    pipeline.emplace(*layout, *pass);
    expect_triangle_color(render([&](vsl::DefaultPhaseStream &stream) {
        stream << vsl::command::BindVertexBuffer(buffer) << vsl::command::Draw(3);
    }));
}
} // namespace

namespace {
TEST_F(VertexStrideVulkanIntegrationTest, RejectedAttributesPreservePreviouslyInjectedDescriptions) {
    auto layout = make_layout_state();
    pl::VertexInput valid({{.layouts = {{vsl::data_format::FloatRG, 0, 4}}}});
    valid.injection(layout);
    pl::VertexInput invalid({{.layouts = {{vsl::data_format::FloatRG, 0,
                                         std::numeric_limits<std::uint32_t>::max() - 2}}}});
    EXPECT_THROW(invalid.injection(layout), vsl::exceptions::RuntimeException);
    const auto &bindings = std::any_cast<const std::vector<vk::VertexInputBindingDescription>&>(
        layout._data->info->pool.at("vertexBindingDescriptions"));
    const auto &attributes = std::any_cast<const std::vector<vk::VertexInputAttributeDescription>&>(
        layout._data->info->pool.at("vertexAttributeDescriptions"));
    ASSERT_EQ(bindings.size(), 1u);
    ASSERT_EQ(attributes.size(), 1u);
    const auto &native = layout._data->info->vertexInput;
    ASSERT_EQ(native.vertexBindingDescriptionCount, 1u);
    ASSERT_EQ(native.vertexAttributeDescriptionCount, 1u);
    EXPECT_EQ(native.pVertexBindingDescriptions, bindings.data());
    EXPECT_EQ(native.pVertexAttributeDescriptions, attributes.data());
    EXPECT_EQ(native.pVertexBindingDescriptions[0].stride, 12u);
    EXPECT_EQ(native.pVertexAttributeDescriptions[0].offset, 4u);
}
} // namespace
