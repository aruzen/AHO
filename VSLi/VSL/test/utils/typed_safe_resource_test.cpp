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
    TEST_F(TypedResourceVulkanIntegrationTest, TypedVertexInputInjectsItsDeclaredAttribute) {
        using Attribute = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRG, 3, 8>;
        using Shape = vsl::typed::VertexShapeDefinition<
            4, 16, vsl::pipeline_layout::VertexInputShapeDefinition::UpdateTiming::NextInstance, Attribute>;
        auto layout = make_layout_state();
        vsl::typed::TypedVertexInput<Shape>{}.injection(layout);
        const auto& state = layout._data->info->vertexInput;
        ASSERT_EQ(state.vertexBindingDescriptionCount, 1u);
        ASSERT_EQ(state.vertexAttributeDescriptionCount, 1u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].binding, 4u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].stride, 16u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].inputRate, vk::VertexInputRate::eInstance);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].location, 3u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].offset, 8u);
    }
} // namespace

#include "support/render_fixture.hpp"
#include <limits>
namespace {
    namespace pl = vsl::pipeline_layout;
    using Timing = pl::VertexInputShapeDefinition::UpdateTiming;
    TEST_F(TypedResourceVulkanIntegrationTest, TypedShapesPreserveOrderingDefaultsAndInferredStridesAfterTheWrapperDies) {
        using Position = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRG, 3, 8>;
        using Color = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRGB>;
        using Scalar = vsl::typed::VertexInputDefinition<vsl::data_format::Float>;
        using FirstShape = vsl::typed::VertexShapeDefinition<4, UINT32_MAX, Timing::NextInstance, Position, Color>;
        using NextShape = vsl::typed::VertexShapeDefinition<UINT32_MAX, UINT32_MAX, Timing::NextVertex, Scalar>;
        auto layout = make_layout_state();
        { vsl::typed::TypedVertexInput<FirstShape, NextShape> input; input.injection(layout); }
        const auto& state = layout._data->info->vertexInput;
        ASSERT_EQ(state.vertexBindingDescriptionCount, 2u);
        ASSERT_EQ(state.vertexAttributeDescriptionCount, 3u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].binding, 4u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].stride, 28u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].inputRate, vk::VertexInputRate::eInstance);
        EXPECT_EQ(state.pVertexBindingDescriptions[1].binding, 5u);
        EXPECT_EQ(state.pVertexBindingDescriptions[1].stride, 4u);
        EXPECT_EQ(state.pVertexBindingDescriptions[1].inputRate, vk::VertexInputRate::eVertex);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].location, 3u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].binding, 4u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].offset, 8u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].format, vk::Format::eR32G32Sfloat);
        EXPECT_EQ(state.pVertexAttributeDescriptions[1].location, 4u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[1].binding, 4u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[1].offset, 16u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[1].format, vk::Format::eR32G32B32Sfloat);
        EXPECT_EQ(state.pVertexAttributeDescriptions[2].location, 5u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[2].binding, 5u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[2].offset, 0u);
    }
    TEST_F(TypedResourceVulkanIntegrationTest, EmptyTypedInputReplacesExistingBindingsAndAttributes) {
        auto layout = make_layout_state();
        pl::VertexInput original;
        original.add(vsl::data_format::FloatRG);
        original.injection(layout);
        pl::ColorBlend{}.injection(layout);
        const auto* colors = layout._data->info->colorBlend.pAttachments;
        vsl::typed::TypedVertexInput<>{}.injection(layout);
        const auto& state = layout._data->info->vertexInput;
        EXPECT_EQ(state.vertexBindingDescriptionCount, 0u);
        EXPECT_EQ(state.vertexAttributeDescriptionCount, 0u);
        EXPECT_EQ(layout._data->info->colorBlend.pAttachments, colors);
    }
    TEST_F(TypedResourceVulkanIntegrationTest, InvalidTypedExtentUsesExistingValidationAndPreservesPreviouslyInjectedData) {
        auto layout = make_layout_state();
        pl::VertexInput original;
        original.add(vsl::data_format::FloatRG);
        original.injection(layout);
        const auto previous = layout._data->info->vertexInput;
        using Overflow = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRG, 0, UINT32_MAX - 3>;
        using Shape = vsl::typed::VertexShapeDefinition<0, UINT32_MAX, Timing::NextVertex, Overflow>;
        EXPECT_THROW(vsl::typed::TypedVertexInput<Shape>{}.injection(layout), vsl::exceptions::RuntimeException);
        const auto& state = layout._data->info->vertexInput;
        EXPECT_EQ(state.vertexBindingDescriptionCount, previous.vertexBindingDescriptionCount);
        EXPECT_EQ(state.pVertexBindingDescriptions, previous.pVertexBindingDescriptions);
        EXPECT_EQ(state.pVertexAttributeDescriptions, previous.pVertexAttributeDescriptions);
    }
    using TypedVertexRenderVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(TypedVertexRenderVulkanIntegrationTest, TypedBindingsSupplyPositionAndColorToTheRealVertexShader) {
        using Position = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRG, 0, 0>;
        using Color = vsl::typed::VertexInputDefinition<vsl::data_format::FloatRGB, 1, 0>;
        using Positions = vsl::typed::VertexShapeDefinition<0, UINT32_MAX, Timing::NextVertex, Position>;
        using Colors = vsl::typed::VertexShapeDefinition<1, UINT32_MAX, Timing::NextVertex, Color>;
        pipeline.reset();
        layout.reset();
        vertex.emplace(device, vsl_test::shader_path("vertices.vert.spv"));
        layout.emplace(device, pl::ShaderGroup("typed-vertices", {*vertex, *fragment}),
                       vsl::typed::TypedVertexInput<Positions, Colors>{}, pl::ColorBlend{}, pl::InputAssembly{},
                       pl::Rasterization{pl::CullingMode::None}, pl::Multisample{}, pl::DynamicState{},
                       vsl::Viewport(description), vsl::Scissor(description));
        ASSERT_EQ(layout->_data->info->vertexInput.vertexBindingDescriptionCount, 2u);
        pipeline.emplace(*layout, *pass);
        std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
        std::array<float, 9> colors{.25f, .5f, .75f, .25f, .5f, .75f, .25f, .5f, .75f};
        vsl::StagingBuffer<vsl::MemoryType::VertexBuffer> positionBuffer(device, positions), colorBuffer(device, colors);
        expect_triangle_color(render([&](auto& stream) {
            stream << vsl::command::BindVertexBuffer(positionBuffer, colorBuffer) << vsl::command::Draw{3};
        }));
    }
} // namespace
