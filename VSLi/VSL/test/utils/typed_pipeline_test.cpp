#include <VSL/utils/typed_pipeline.hpp>
#include "support/render_fixture.hpp"
#include <cstddef>
#include <limits>
#include "support/temporary_directory.hpp"

namespace {
    namespace typed = vsl::typed;
    using Timing = typed::VertexTiming;
    struct Position { float xy[2]; };
    struct Color { float rgb[3]; };
    using PositionAttribute = typed::VertexInputDefinition<vsl::data_format::FLOAT_RG, 0, offsetof(Position, xy)>;
    using ColorAttribute = typed::VertexInputDefinition<vsl::data_format::FLOAT_RGB, 1, offsetof(Color, rgb)>;
    using PositionRecord = typed::VertexRecord<Position, VSL_VERTEX_MEMBER(Position, xy)>;
    using ColorRecord = typed::VertexRecord<Color, VSL_VERTEX_MEMBER(Color, rgb)>;
    using Positions = PositionRecord::Binding<0>;
    using Colors = ColorRecord::Binding<1, Timing::NEXT_VERTEX, 1>;
    using Input = typed::TypedVertexInput<Positions, Colors>;
    static_assert(Input::LAYOUT.binding[0].stride == sizeof(Position));
    static_assert(Input::LAYOUT.binding[1].stride == sizeof(Color));
    static_assert(Input::LAYOUT.attribute[1].location == 1);
    static_assert(typed::compatible_vertex_buffers<Input, typed::TypedVertexBuffer<Position>, typed::TypedVertexBuffer<Color>>);
    static_assert(!typed::compatible_vertex_buffers<Input, typed::TypedVertexBuffer<Color>, typed::TypedVertexBuffer<Position>>);
    static_assert(!typed::compatible_vertex_buffers<Input, typed::TypedVertexBuffer<Position>>);
    static_assert(!typed::compatible_vertex_buffers<Input, int, int>);
    static_assert(typed::compatible_vertex_buffers<typed::TypedVertexInput<>>);
    using RawShape = typed::VertexShapeDefinition<0, 8, Timing::NEXT_VERTEX, PositionAttribute>;
    static_assert(!typed::compatible_vertex_buffers<typed::TypedVertexInput<RawShape>, typed::TypedVertexBuffer<Position>>);
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

    TEST(TypedVertexLayoutTest, RecordSchemaInfersMembersAndReusesLocationsWithPadding) {
        struct Vertex { std::uint8_t tag; float xy[2]; std::array<double, 3> normal; };
        using Schema = typed::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy), VSL_VERTEX_MEMBER(Vertex, normal)>;
        using Binding = Schema::Binding<7, Timing::NEXT_INSTANCE, 3>;
        constexpr auto layout = typed::TypedVertexInput<Binding>::LAYOUT;
        static_assert(layout.attribute[0].format == vsl::data_format::FLOAT_RG);
        static_assert(layout.attribute[1].format == vsl::data_format::FLOAT64_RGB);
        static_assert(layout.attribute[0].offset == offsetof(Vertex, xy));
        static_assert(layout.attribute[1].offset == offsetof(Vertex, normal));
        static_assert(layout.attribute[0].location == 3 && layout.attribute[1].location == 4);
        static_assert(layout.attribute[1].locations == 2 && layout.binding[0].stride == sizeof(Vertex));
        EXPECT_EQ(layout.binding[0].timing, Timing::NEXT_INSTANCE);
    }

    struct GraphicVector {
        float xy[2];
        static constexpr auto GRAPHIC_TYPE = vsl::data_format::FLOAT_RG;
    };
    TEST(TypedVertexLayoutTest, SignedByteAndCustomGraphicTypesInferTheirStorageFormats) {
        struct Vertex { std::int8_t signed_values[3]; std::array<std::uint8_t, 4> unsigned_values; GraphicVector vector; };
        using Schema = typed::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, signed_values),
            VSL_VERTEX_MEMBER(Vertex, unsigned_values), VSL_VERTEX_MEMBER(Vertex, vector)>;
        static_assert(Schema::ATTRIBUTES[0].format == vsl::data_format::INT8_RGB);
        static_assert(Schema::ATTRIBUTES[1].format == vsl::data_format::UNSIGNED_INT8_RGBA);
        static_assert(Schema::ATTRIBUTES[2].format == vsl::data_format::FLOAT_RG);
        EXPECT_EQ(Schema::LAYOUT.binding[0].stride, sizeof(Vertex));
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
        typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Color> colors(device, COLORS);
        auto bindings = typed_pipeline.bind_vertices(positions, colors);
        static_assert(std::same_as<typename decltype(bindings)::VertexInput, Input>);
        ExpectTriangleColor(render([&](auto& stream) { stream << bindings.draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, SparseBindingsAreNotRenumberedDuringRecording) {
        using SparsePositions = PositionRecord::Binding<4>;
        using SparseColors = ColorRecord::Binding<7, Timing::NEXT_VERTEX, 1>;
        auto typed_pipeline = make_typed<typed::TypedVertexInput<SparsePositions, SparseColors>>();
        typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Color> colors(device, COLORS);
        auto original_pipeline = make_typed<Input>();
        const auto owner = positions.backend().data_;
        ExpectTriangleColor(render([&](auto& stream) { stream << original_pipeline.bind_vertices(positions, colors).draw(3); }));
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions, colors).draw(3); }));
        EXPECT_EQ(owner, positions.backend().data_) << "Binding changes must reuse the same GPU allocation";
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, InstanceRateAndFirstVertexAndInstanceSelectTheCorrectRecords) {
        using Instances = ColorRecord::Binding<1, Timing::NEXT_INSTANCE, 1>;
        auto typed_pipeline = make_typed<typed::TypedVertexInput<Positions, Instances>>();
        const std::array<Position, 4> source{{{{0, 0}}, POSITIONS[0], POSITIONS[1], POSITIONS[2]}};
        const std::array<Color, 2> tints{{{{1, 0, 0}}, COLORS[0]}};
        typed::TypedVertexBuffer<Position> positions(device, source);
        typed::TypedVertexBuffer<Color> colors(device, tints);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions, colors).draw(3, 1, 1, 1); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, EmptyInputSupportsProceduralVertices) {
        auto typed_pipeline = make_typed<typed::TypedVertexInput<>>("triangle.vert.spv");
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices().draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, DrawRejectsOutOfBoundsRecordsBeforeRecording) {
        auto typed_pipeline = make_typed<Input>();
        typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Color> colors(device, COLORS);
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
                typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
                const std::array<Color, 3> tints{{{{.8f, .2f, .4f}}, {{.8f, .2f, .4f}}, {{.8f, .2f, .4f}}}};
                typed::TypedVertexBuffer<Color> colors(device, tints);
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
        typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Color, vsl::MemoryProperty::DEVICE_LOCAL> colors(device, make_manager(), COLORS);
        EXPECT_EQ(positions.size(), 3u);
        positions.copy(POSITIONS);
        EXPECT_THROW(positions.copy(std::span<const Position>(POSITIONS).first(2)), std::invalid_argument);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions, colors).draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, InterleavedFloatAndNormalizedIntegerAttributesKeepPadding) {
        struct Vertex { float xy[2]; std::uint8_t rgba[4]; float padding; };
        using Schema = typed::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy),
            VSL_VERTEX_MEMBER(Vertex, rgba, typed::AUTO, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA)>;
        using Record = Schema::Binding<3>;
        using Layout = typed::TypedVertexInput<Record>;
        static_assert(Layout::LAYOUT.binding[0].stride == sizeof(Vertex));
        auto typed_pipeline = make_typed<Layout>();
        const std::array<Vertex, 3> records{{{{-1, -1}, {64, 128, 191, 255}, 123},
                                            {{3, -1}, {64, 128, 191, 255}, 456},
                                            {{-1, 3}, {64, 128, 191, 255}, 789}}};
        typed::TypedVertexBuffer<Vertex> buffer(device, records);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(buffer).draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, ExistingDynamicApiCanUseExplicitBackendAccess) {
        auto typed_pipeline = make_typed<Input>();
        typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Color> colors(device, COLORS);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << typed_pipeline.backend() << vsl::command::BindVertexBuffer(positions.backend(), colors.backend())
                   << vsl::command::Draw(3);
        }));
    }
    TEST_F(TypedPipelineVulkanIntegrationTest, ShaderMismatchIsRejectedBeforeNativeGraphicsPipelineCreation) {
        using Empty = typed::TypedVertexInput<>;
        EXPECT_THROW(make_typed<Empty>(), std::invalid_argument);
        struct UnsignedPosition { std::uint32_t xy[2]; };
        using Schema = typed::VertexRecord<UnsignedPosition, VSL_VERTEX_MEMBER(UnsignedPosition, xy)>;
        using Wrong = typed::TypedVertexInput<Schema::Binding<0>, Colors>;
        EXPECT_THROW(make_typed<Wrong>(), std::invalid_argument);
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, ValidationUsesLoadedModuleEvenAfterTheSourceFileIsRemoved) {
        vsl_test::TemporaryDirectory directory;
        const auto path = directory.path() / "loaded.vert.spv";
        std::filesystem::copy_file(vsl_test::shader_path("vertices.vert.spv"), path);
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, path);
        std::filesystem::remove(path);
        namespace pl = vsl::pipeline_layout;
        typed::TypedGraphicsPipeline<Input> typed_pipeline(device, *pass,
            pl::ShaderGroup("removed source", {shader, *fragment}), pl::ColorBlend{}, pl::InputAssembly{},
            pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{},
            vsl::Viewport(description), vsl::Scissor(description));
        typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
        typed::TypedVertexBuffer<Color> colors(device, COLORS);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions, colors).draw(3); }));
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, MissingAndDuplicateVertexStagesAreRejectedBeforeDriverPipelineCreation) {
        namespace pl = vsl::pipeline_layout;
        EXPECT_THROW((typed::TypedGraphicsPipeline<Input>(device, *pass, pl::ShaderGroup("no vertex", {*fragment}))), std::invalid_argument);
        EXPECT_THROW((typed::TypedGraphicsPipeline<Input>(device, *pass, pl::ShaderGroup("duplicate vertex", {*vertex, *vertex, *fragment}))), std::invalid_argument);
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(TypedPipelineVulkanIntegrationTest, ActualNamedEntryPointIsValidatedAndUsedForDrawing) {
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path("typed_entry_points.spv"), "second");
        auto make = [&]<typename Layout>() {
            return typed::TypedGraphicsPipeline<Layout>(device, *pass, pl::ShaderGroup("entry second", {shader, *fragment}),
                pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{},
                pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        };
        using Wrong = typed::TypedVertexInput<PositionRecord::Binding<0>>;
        using Correct = typed::TypedVertexInput<PositionRecord::Binding<3, Timing::NEXT_VERTEX, 7>>;
        EXPECT_THROW(make.template operator()<Wrong>(), std::invalid_argument);
        auto typed_pipeline = make.template operator()<Correct>();
        typed::TypedVertexBuffer<Position> positions(device, POSITIONS);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices(positions).draw(3); }));
    }

}
