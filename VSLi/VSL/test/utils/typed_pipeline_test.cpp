#include <VSL/utils/typed_pipeline.hpp>
#include "support/render_fixture.hpp"
#include <cstddef>
#include <limits>

namespace {
    namespace typed = vsl::typed;
    using Timing = typed::VertexTiming;
    struct Position { float xy[2]; };
    struct Color { float rgb[3]; };
    using PositionAttribute = typed::VertexInputDefinition<vsl::data_format::FLOAT_RG, 0, offsetof(Position, xy)>;
    using ColorAttribute = typed::VertexInputDefinition<vsl::data_format::FLOAT_RGB, 1, offsetof(Color, rgb)>;
    using Positions = typed::VertexBinding<Position, 0, Timing::NEXT_VERTEX, PositionAttribute>;
    using Colors = typed::VertexBinding<Color, 1, Timing::NEXT_VERTEX, ColorAttribute>;
    using Input = typed::TypedVertexInput<Positions, Colors>;
    static_assert(Input::LAYOUT.binding[0].stride == sizeof(Position));
    static_assert(Input::LAYOUT.binding[1].stride == sizeof(Color));
    static_assert(Input::LAYOUT.attribute[1].location == 1);
    static_assert(typed::compatible_vertex_buffers<Input, typed::TypedVertexBuffer<Positions>, typed::TypedVertexBuffer<Colors>>);
    static_assert(!typed::compatible_vertex_buffers<Input, typed::TypedVertexBuffer<Colors>, typed::TypedVertexBuffer<Positions>>);
    static_assert(!typed::compatible_vertex_buffers<Input, typed::TypedVertexBuffer<Positions>>);
    static_assert(!typed::compatible_vertex_buffers<Input, int, int>);
    static_assert(typed::compatible_vertex_buffers<typed::TypedVertexInput<>>);
    static_assert(std::same_as<typed::TypedGraphicsPipeline<Input>::VertexInput, Input>);

    const std::array<Position, 3> POSITIONS{{{{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
    const std::array<Color, 3> COLORS{{{{.25f, .5f, .75f}}, {{.25f, .5f, .75f}}, {{.25f, .5f, .75f}}}};

    TEST(TypedVertexLayoutTest, ResolvesAutomaticMetadataDuringConstantEvaluation) {
        using First = typed::VertexShapeDefinition<4, typed::AUTO, Timing::NEXT_INSTANCE,
            typed::VertexInputDefinition<vsl::data_format::FLOAT_RG, 3, 8>,
            typed::VertexInputDefinition<vsl::data_format::FLOAT_RGB>>;
        using Second = typed::VertexShapeDefinition<typed::AUTO, typed::AUTO, Timing::NEXT_VERTEX,
            typed::VertexInputDefinition<vsl::data_format::FLOAT>>;
        constexpr auto layout = typed::TypedVertexInput<First, Second>::LAYOUT;
        static_assert(layout.binding[0].binding == 4 && layout.binding[0].stride == 28);
        static_assert(layout.binding[1].binding == 5 && layout.binding[1].stride == 4);
        static_assert(layout.attribute[0].location == 3 && layout.attribute[0].offset == 8);
        static_assert(layout.attribute[1].location == 4 && layout.attribute[1].offset == 16);
        static_assert(layout.attribute[2].location == 5 && layout.attribute[2].offset == 0);
        EXPECT_EQ(layout.binding[0].timing, Timing::NEXT_INSTANCE);
    }

    TEST(TypedVertexLayoutTest, WideAttributesReserveTwoLocationsAndPaddingSetsRecordStride) {
        struct Wide { double vector[3]; float scalar; };
        using Binding = typed::VertexBinding<Wide, 0, Timing::NEXT_VERTEX,
            typed::VertexInputDefinition<vsl::data_format::FLOAT64_RGB, 2, offsetof(Wide, vector)>,
            typed::VertexInputDefinition<vsl::data_format::FLOAT, typed::AUTO, offsetof(Wide, scalar)>>;
        constexpr auto layout = typed::TypedVertexInput<Binding>::LAYOUT;
        static_assert(layout.attribute[0].locations == 2 && layout.attribute[1].location == 4);
        static_assert(layout.binding[0].stride == sizeof(Wide));
        EXPECT_GT(sizeof(Wide), offsetof(Wide, scalar) + sizeof(float));
    }

    class TypedPipelineVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    protected:
        template<typename Layout>
        auto make_typed(const char* shader = "vertices.vert.spv") {
            namespace pl = vsl::pipeline_layout;
            vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path(shader));
            return typed::TypedGraphicsPipeline<Layout>(device, *pass,
                pl::ShaderGroup("typed pipeline", {source, *fragment}), pl::ColorBlend{}, pl::InputAssembly{},
                pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{},
                vsl::Viewport(description), vsl::Scissor(description));
        }
    };

    TEST_F(TypedPipelineVulkanIntegrationTest, TypedPipelineAndBuffersProduceTheExpectedPixel) {
        auto typed_pipeline = make_typed<Input>();
        typed::TypedVertexBuffer<Positions> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Colors> colors(device, COLORS);
        auto bindings = typed_pipeline.bind_vertices(positions, colors);
        static_assert(std::same_as<typename decltype(bindings)::VertexInput, Input>);
        ExpectTriangleColor(render([&](auto& stream) { stream << bindings.draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, SparseBindingsAreNotRenumberedDuringRecording) {
        using SparsePositions = typed::VertexBinding<Position, 4, Timing::NEXT_VERTEX, PositionAttribute>;
        using SparseColors = typed::VertexBinding<Color, 7, Timing::NEXT_VERTEX, ColorAttribute>;
        auto typed_pipeline = make_typed<typed::TypedVertexInput<SparsePositions, SparseColors>>();
        typed::TypedVertexBuffer<SparsePositions> positions(device, POSITIONS);
        typed::TypedVertexBuffer<SparseColors> colors(device, COLORS);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions, colors).draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, InstanceRateAndFirstVertexAndInstanceSelectTheCorrectRecords) {
        using Instances = typed::VertexBinding<Color, 1, Timing::NEXT_INSTANCE, ColorAttribute>;
        auto typed_pipeline = make_typed<typed::TypedVertexInput<Positions, Instances>>();
        const std::array<Position, 4> source{{{{0, 0}}, POSITIONS[0], POSITIONS[1], POSITIONS[2]}};
        const std::array<Color, 2> tints{{{{1, 0, 0}}, COLORS[0]}};
        typed::TypedVertexBuffer<Positions> positions(device, source);
        typed::TypedVertexBuffer<Instances> colors(device, tints);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions, colors).draw(3, 1, 1, 1); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, EmptyInputSupportsProceduralVertices) {
        auto typed_pipeline = make_typed<typed::TypedVertexInput<>>("triangle.vert.spv");
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices().draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, DrawRejectsOutOfBoundsRecordsBeforeRecording) {
        auto typed_pipeline = make_typed<Input>();
        typed::TypedVertexBuffer<Positions> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Colors> colors(device, COLORS);
        auto bindings = typed_pipeline.bind_vertices(positions, colors);
        EXPECT_THROW(bindings.draw(4), std::out_of_range);
        EXPECT_THROW(bindings.draw(3, 1, 1), std::out_of_range);
        EXPECT_THROW(bindings.draw(1, 1, std::numeric_limits<std::uint32_t>::max()), std::out_of_range);
        if constexpr (sizeof(size_t) > sizeof(std::uint32_t))
            EXPECT_THROW(bindings.draw(size_t{UINT32_MAX} + 1), vsl::exceptions::RuntimeException);
        ExpectTriangleColor(render([&](auto& stream) { stream << bindings.draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, CommandsOwnResourcesAndRebindTheirPipelineAfterDynamicCommands) {
        std::weak_ptr<vsl::impl::BufferImpl> weak;
        const auto pixel = render([&](auto& stream) {
            {
                auto typed_pipeline = make_typed<Input>();
                typed::TypedVertexBuffer<Positions> positions(device, POSITIONS);
                const std::array<Color, 3> tints{{{{.8f, .2f, .4f}}, {{.8f, .2f, .4f}}, {{.8f, .2f, .4f}}}};
                typed::TypedVertexBuffer<Colors> colors(device, tints);
                weak = positions.backend().data_;
                auto command = typed_pipeline.bind_vertices(positions, colors).draw(3);
                stream << *pipeline << command;
            }
            EXPECT_FALSE(weak.expired()) << "GPU recording must retain the buffer after all wrappers and commands die";
        });
        EXPECT_NEAR(pixel[0], 204, 1);
        EXPECT_NEAR(pixel[1], 51, 1);
        EXPECT_NEAR(pixel[2], 102, 1);
        EXPECT_EQ(pixel[3], 255);
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, TypedUpdatesKeepRecordCountAndDeviceLocalUploadIsSupported) {
        auto typed_pipeline = make_typed<Input>();
        typed::TypedVertexBuffer<Positions> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Colors, vsl::MemoryProperty::DEVICE_LOCAL> colors(device, make_manager(), COLORS);
        EXPECT_EQ(positions.size(), 3u);
        positions.copy(POSITIONS);
        EXPECT_THROW(positions.copy(std::span<const Position>(POSITIONS).first(2)), std::invalid_argument);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions, colors).draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, InterleavedFloatAndNormalizedIntegerAttributesKeepPadding) {
        struct Vertex { float xy[2]; std::uint8_t rgba[4]; float padding; };
        using Record = typed::VertexBinding<Vertex, 3, Timing::NEXT_VERTEX,
            typed::VertexInputDefinition<vsl::data_format::FLOAT_RG, 0, offsetof(Vertex, xy)>,
            typed::VertexInputDefinition<vsl::data_format::UNSIGNED_NORMALIZED8_RGBA, 1, offsetof(Vertex, rgba)>>;
        using Layout = typed::TypedVertexInput<Record>;
        static_assert(Layout::LAYOUT.binding[0].stride == sizeof(Vertex));
        auto typed_pipeline = make_typed<Layout>();
        const std::array<Vertex, 3> records{{{{-1, -1}, {64, 128, 191, 255}, 123},
                                            {{3, -1}, {64, 128, 191, 255}, 456},
                                            {{-1, 3}, {64, 128, 191, 255}, 789}}};
        typed::TypedVertexBuffer<Record> buffer(device, records);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(buffer).draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, ExistingDynamicApiCanUseExplicitBackendAccess) {
        auto typed_pipeline = make_typed<Input>();
        typed::TypedVertexBuffer<Positions> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Colors> colors(device, COLORS);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << typed_pipeline.backend() << vsl::command::BindVertexBuffer(positions.backend(), colors.backend())
                   << vsl::command::Draw(3);
        }));
    }
}
