#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    TEST(VertexInputTest, AddsFormatsShapesAndExplicitDefinitionsWithExpectedSizes) {
        pl::VertexInput input;
        EXPECT_EQ(input.requirements_size(), 0u);
        input.add(vsl::data_format::FLOAT_RG);
        input.add({vsl::data_format::FLOAT_RGB, vsl::data_format::UNSIGNED_INT});
        EXPECT_EQ(input.definitions.size(), 3u);
        EXPECT_EQ(input.requirements_size(), 24u);
        input.add_shape({vsl::data_format::FLOAT_RG, vsl::data_format::FLOAT_RGB});
        ASSERT_EQ(input.definitions.size(), 4u);
        EXPECT_EQ(input.definitions.back().layouts.size(), 2u);
        EXPECT_EQ(input.requirements_size(), 44u);
        input.add(pl::VertexInputShapeDefinition{.layouts = {{vsl::data_format::FLOAT}}, .size = 16});
        EXPECT_EQ(input.requirements_size(), 60u);
    }
    TEST(VertexInputTest, InitializesAndPrependsShapeDefinitions) {
        const pl::VertexInputShapeDefinition first{.layouts = {{vsl::data_format::FLOAT_RG}}, .binding = 2};
        const pl::VertexInputShapeDefinition second{.layouts = {{vsl::data_format::FLOAT_RGB}}, .binding = 5};
        pl::VertexInput input({first});
        input.add(std::initializer_list<pl::VertexInputShapeDefinition>{second});
        ASSERT_EQ(input.definitions.size(), 2u);
        EXPECT_EQ(input.definitions[0].binding, 5u);
        EXPECT_EQ(input.definitions[1].binding, 2u);
    }
    using VertexInputVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(VertexInputVulkanIntegrationTest, WideAttributesReserveBothLocationsWithinAndAcrossBindings) {
        using enum vsl::data_format::FormatTypes;
        for (const auto type : {SIGNED_FLOAT64_RGB, SIGNED_FLOAT64_RGBA, SIGNED_INT64_RGB, SIGNED_INT64_RGBA, UNSIGNED_INT64_RGB, UNSIGNED_INT64_RGBA}) {
            const auto format = vsl::data_format::convert(type);
            SCOPED_TRACE(static_cast<int>(type));
            pl::VertexInput input;
            input.add_shape({format, vsl::data_format::FLOAT});
            input.add_shape({format, vsl::data_format::FLOAT});
            auto layout = make_layout_state();
            input.injection(layout);
            const auto& info = layout.data_->info->vertex_input;
            ASSERT_EQ(info.vertexAttributeDescriptionCount, 4u);
            for (std::size_t i = 0; i < 4; ++i) {
                EXPECT_EQ(info.pVertexAttributeDescriptions[i].location, (std::array{0u, 2u, 3u, 5u})[i]);
                EXPECT_EQ(info.pVertexAttributeDescriptions[i].binding, i / 2);
            }
            EXPECT_EQ(info.pVertexAttributeDescriptions[1].offset, format.size());
            EXPECT_EQ(info.pVertexAttributeDescriptions[3].offset, format.size());
            pl::VertexInput explicit_input({{.layouts = {{format, 3}, {vsl::data_format::FLOAT}}},
                                            {.layouts = {{format, 9}}},
                                            {.layouts = {{vsl::data_format::FLOAT}}}});
            explicit_input.injection(layout);
            const auto& explicit_info = layout.data_->info->vertex_input;
            ASSERT_EQ(explicit_info.vertexAttributeDescriptionCount, 4u);
            for (std::size_t i = 0; i < 4; ++i)
                EXPECT_EQ(explicit_info.pVertexAttributeDescriptions[i].location, (std::array{3u, 5u, 9u, 11u})[i]);
        }
    }
    TEST_F(VertexInputVulkanIntegrationTest, ScalarAndPair64BitAndFourComponent32BitFormatsKeepOneLocation) {
        using enum vsl::data_format::FormatTypes;
        for (const auto type : {SIGNED_FLOAT64_R, SIGNED_FLOAT64_RG, SIGNED_INT64_R, SIGNED_INT64_RG, UNSIGNED_INT64_R, UNSIGNED_INT64_RG,
                                SIGNED_FLOAT32_RGBA, SIGNED_INT32_RGBA, UNSIGNED_INT32_RGBA}) {
            const auto format = vsl::data_format::convert(type);
            pl::VertexInput input;
            input.add_shape({format, vsl::data_format::FLOAT});
            auto layout = make_layout_state();
            input.injection(layout);
            const auto& info = layout.data_->info->vertex_input;
            ASSERT_EQ(info.vertexAttributeDescriptionCount, 2u);
            EXPECT_EQ(info.pVertexAttributeDescriptions[0].location, 0u);
            EXPECT_EQ(info.pVertexAttributeDescriptions[1].location, 1u);
        }
    }
    TEST_F(VertexInputVulkanIntegrationTest, LocationExtentOverflowPreservesPreviouslyInjectedDescriptions) {
        auto layout = make_layout_state();
        pl::VertexInput valid;
        valid.add(vsl::data_format::FLOAT);
        valid.injection(layout);
        const auto previous = layout.data_->info->vertex_input;
        pl::VertexInput wide({{.layouts = {{vsl::data_format::FLOAT64_RGB, UINT32_MAX - 1}}}});
        EXPECT_THROW(wide.injection(layout), vsl::exceptions::RuntimeException);
        pl::VertexInput automatic({{.layouts = {{vsl::data_format::FLOAT, UINT32_MAX - 1}, {vsl::data_format::FLOAT}}}});
        EXPECT_THROW(automatic.injection(layout), vsl::exceptions::RuntimeException);
        EXPECT_EQ(layout.data_->info->vertex_input.pVertexAttributeDescriptions, previous.pVertexAttributeDescriptions);
        EXPECT_EQ(layout.data_->info->vertex_input.pVertexBindingDescriptions, previous.pVertexBindingDescriptions);
        EXPECT_EQ(layout.data_->info->vertex_input.vertexAttributeDescriptionCount, 1u);
    }
    TEST_F(VertexInputVulkanIntegrationTest, InjectsExplicitAndAutomaticLocationsStridesAndOffsets) {
        pl::VertexInput input({{.layouts = {{vsl::data_format::FLOAT_RG, 3, 4}, {vsl::data_format::FLOAT_RGB}},
                                .binding = 2,
                                .size = 32,
                                .update_timing = pl::VertexInputShapeDefinition::UpdateTiming::NEXT_INSTANCE}});
        auto layout = make_layout_state();
        input.injection(layout);
        const auto& info = layout.data_->info->vertex_input;
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
        EXPECT_EQ(input.definitions[0].layouts[0].format, vsl::data_format::FLOAT);
        EXPECT_EQ(result.definitions[0].layouts[0].format, vsl::data_format::FLOAT);
        EXPECT_EQ(input.requirements_size(), 4u);
    }
    TEST(VertexInputTest, TypePacksPreserveArgumentOrderAndPreviouslyAddedShapes) {
        pl::VertexInput input;
        input.add(vsl::data_format::FLOAT_RG);
        input.add<float, std::uint32_t>();
        ASSERT_EQ(input.definitions.size(), 3u);
        EXPECT_EQ(input.definitions[0].layouts[0].format, vsl::data_format::FLOAT_RG);
        EXPECT_EQ(input.definitions[1].layouts[0].format, vsl::data_format::FLOAT);
        EXPECT_EQ(input.definitions[2].layouts[0].format, vsl::data_format::UNSIGNED_INT);
        EXPECT_EQ(input.requirements_size(), 16u);
    }
} // namespace

TEST(VertexInputTest, EmptyTypePacksLeaveExistingShapesUnchanged) {
    pl::VertexInput input;
    input.add(vsl::data_format::FLOAT_RG);
    const auto result = input.add<>();
    ASSERT_EQ(input.definitions.size(), 1u);
    ASSERT_EQ(result.definitions.size(), 1u);
    EXPECT_EQ(input.definitions[0].layouts[0].format, vsl::data_format::FLOAT_RG);
    EXPECT_EQ(input.requirements_size(), 8u);
}

#include <limits>
namespace {
    TEST(VertexInputTest, InferredRequirementsIncludeExplicitGapsAndTheLargestAttributeEnd) {
        pl::VertexInput input({{.layouts = {{vsl::data_format::FLOAT_RG, 0, 24},
                                            {vsl::data_format::FLOAT, 1, 0},
                                            {vsl::data_format::FLOAT, 2}}}});
        EXPECT_EQ(input.requirements_size(), 32u);
        input.add(pl::VertexInputShapeDefinition{.layouts = {{vsl::data_format::FLOAT, 3, 12}}});
        EXPECT_EQ(input.requirements_size(), 48u);
    }
    using VertexStrideVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(VertexStrideVulkanIntegrationTest, NativeStridesUseTheLargestEndAndPreserveExplicitSize) {
        pl::VertexInput input({{.layouts = {{vsl::data_format::FLOAT_RG, 0, 24},
                                            {vsl::data_format::FLOAT, 1, 0},
                                            {vsl::data_format::FLOAT, 2}}},
                               {.layouts = {{vsl::data_format::FLOAT, 3, 4}}, .size = 20}});
        auto layout = make_layout_state();
        input.injection(layout);
        const auto& info = layout.data_->info->vertex_input;
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
        pl::VertexInput input({{.layouts = {{vsl::data_format::FLOAT_RG, 0,
                                             std::numeric_limits<std::uint32_t>::max() - 2}}}});
        EXPECT_THROW(input.requirements_size(), vsl::exceptions::RuntimeException);
        auto layout = make_layout_state();
        EXPECT_THROW(input.injection(layout), vsl::exceptions::RuntimeException);
    }
} // namespace
#include "support/render_fixture.hpp"
namespace {
    using VertexStrideRenderVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(VertexStrideRenderVulkanIntegrationTest, PaddedInterleavedVerticesProduceTheExpectedGPUColor) {
        struct Vertex {
            float padding;
            float position[2];
            float color[3];
        };
        const std::array<Vertex, 3> vertices{{{0, {-0.75f, -0.75f}, {0.25f, 0.5f, 0.75f}},
                                              {0, {0.75f, -0.75f}, {0.25f, 0.5f, 0.75f}},
                                              {0, {0, 0.75f}, {0.25f, 0.5f, 0.75f}}}};
        static_assert(sizeof(Vertex) == 24);
        vsl::StagingBuffer<vsl::MemoryType::VERTEX_BUFFER> buffer(device, vertices);
        pipeline.reset();
        layout.reset();
        vertex.emplace(device, vsl_test::shader_path("vertices.vert.spv"));
        pl::VertexInput input({{.layouts = {{vsl::data_format::FLOAT_RG, 0, 4},
                                            {vsl::data_format::FLOAT_RGB, 1}}}});
        EXPECT_EQ(input.requirements_size(), sizeof(Vertex));
        layout.emplace(device, pl::ShaderGroup("padded vertices", {*vertex, *fragment}), input, pl::ColorBlend{},
                       pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{},
                       pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        pipeline.emplace(*layout, *pass);
        ExpectTriangleColor(render([&](vsl::DefaultPhaseStream& stream) {
            stream << vsl::command::BindVertexBuffer(buffer) << vsl::command::Draw(3);
        }));
    }
} // namespace

namespace {
    TEST_F(VertexStrideVulkanIntegrationTest, RejectedAttributesPreservePreviouslyInjectedDescriptions) {
        auto layout = make_layout_state();
        pl::VertexInput valid({{.layouts = {{vsl::data_format::FLOAT_RG, 0, 4}}}});
        valid.injection(layout);
        pl::VertexInput invalid({{.layouts = {{vsl::data_format::FLOAT_RG, 0,
                                               std::numeric_limits<std::uint32_t>::max() - 2}}}});
        EXPECT_THROW(invalid.injection(layout), vsl::exceptions::RuntimeException);
        const auto& bindings = std::any_cast<const std::vector<vk::VertexInputBindingDescription>&>(
            layout.data_->info->pool.at("vertexBindingDescriptions"));
        const auto& attributes = std::any_cast<const std::vector<vk::VertexInputAttributeDescription>&>(
            layout.data_->info->pool.at("vertexAttributeDescriptions"));
        ASSERT_EQ(bindings.size(), 1u);
        ASSERT_EQ(attributes.size(), 1u);
        const auto& native = layout.data_->info->vertex_input;
        ASSERT_EQ(native.vertexBindingDescriptionCount, 1u);
        ASSERT_EQ(native.vertexAttributeDescriptionCount, 1u);
        EXPECT_EQ(native.pVertexBindingDescriptions, bindings.data());
        EXPECT_EQ(native.pVertexAttributeDescriptions, attributes.data());
        EXPECT_EQ(native.pVertexBindingDescriptions[0].stride, 12u);
        EXPECT_EQ(native.pVertexAttributeDescriptions[0].offset, 4u);
    }
} // namespace
