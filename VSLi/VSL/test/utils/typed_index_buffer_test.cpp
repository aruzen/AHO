#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_batch.hpp>
#include "support/render_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    struct Position {
        float xy[2];
    };
    struct Color {
        float rgb[3];
    };
    using Positions = t::VertexRecord<Position, VSL_VERTEX_MEMBER(Position, xy)>;
    using Colors = t::VertexRecord<Color, VSL_VERTEX_MEMBER(Color, rgb)>;
    using Input = t::TypedVertexInput<Positions::Binding<0>, Colors::Binding<1, t::VertexTiming::NEXT_INSTANCE, 1>>;
    const std::array<Position, 3> POSITIONS{{{{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
    const std::array<Color, 2> COLORS{{{{1, 0, 0}}, {{.25f, .5f, .75f}}}};
    class TypedIndexVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        t::TypedGraphicsPipeline<Input> make_typed();
    };
    t::TypedGraphicsPipeline<Input> TypedIndexVulkanIntegrationTest::make_typed() {
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path("vertices.vert.spv"));
        return t::TypedGraphicsPipeline<Input>(device, *pass, pl::ShaderGroup("indexed", {shader, *fragment}),
                                               pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{},
                                               pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
    }
    TEST(TypedIndexTest, NativeDrawRejectsNarrowingInEveryArgument) {
        using Draw = vsl::command::DrawIndexed;
        const auto overflow = size_t(UINT32_MAX) + 1;
        EXPECT_THROW(Draw(overflow, 1), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, overflow), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, overflow), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, 0, std::int64_t(INT32_MAX) + 1), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, 0, std::int64_t(INT32_MIN) - 1), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, 0, 0, overflow), vsl::exceptions::RuntimeException);
    }
    TEST_F(TypedIndexVulkanIntegrationTest, Uint16DrawUsesFirstIndexNegativeBaseVertexAndFirstInstance) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 5> values{99, 1, 2, 3, 99};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 1, -1, 1); }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, Uint32DeviceLocalIndicesAboveUint16RangeArePreserved) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint32_t, 3> values{65536, 65537, 65538};
        auto manager = make_manager();
        t::TypedIndexBuffer<std::uint32_t, vsl::MemoryProperty::DEVICE_LOCAL> indices(device, manager, values);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 0, -65536, 1); }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, InvalidIndexVertexAndInstanceRangesAreRejectedBeforeRecording) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        auto binding = typed.bind_vertices(positions, colors);
        EXPECT_THROW(binding.draw_indexed(indices, 4), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 1, 1, 3), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 3, 1, 0, -1), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 3, 1, 0, 1), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 3, 2, 0, 0, 1), std::out_of_range);
        EXPECT_NO_THROW(binding.draw_indexed(indices, 0, 0, indices.size()));
        const std::array<std::uint16_t, 3> changed{0, 1, 20};
        auto alias = indices;
        alias.copy(changed);
        EXPECT_THROW(binding.draw_indexed(indices, 3), std::out_of_range);
        EXPECT_THROW(indices.copy(std::span(values).first(2)), std::invalid_argument);
        EXPECT_THROW(vsl::command::BindIndexBufferAt(indices.backend(), vsl::command::IndexType::UINT16, 1), std::invalid_argument);
        EXPECT_FALSE(has_validation_errors());
    }
    TEST_F(TypedIndexVulkanIntegrationTest, CapturedCommandRejectsIndexUpdatesBeforeRecording) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 3> values{0, 1, 2}, changed{0, 2, 1};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        const auto binding = typed.bind_vertices(positions, colors);
        auto command = binding.draw_indexed(indices, 3, 1, 0, 0, 1);
        auto alias = indices;
        // 修正前の再現でも、driverへ渡すのは有効なindexだけにする。
        alias.copy(changed);
        ExpectTriangleColor(render([&](auto& stream) {
            EXPECT_THROW(stream << command, std::logic_error);
            EXPECT_FALSE(positions.resource_use().state->busy());
            EXPECT_FALSE(indices.resource_use().state->busy());
            stream << binding.draw_indexed(indices, 3, 1, 0, 0, 1);
        }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, BatchRejectsUpdatedIndicesBeforeAnyResourceIsUsed) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint32_t, 3> values{0, 1, 2}, changed{0, 2, 1};
        t::TypedIndexBuffer<std::uint32_t> first(device, values), second(device, values);
        const auto binding = typed.bind_vertices(positions, colors);
        auto batch = t::TypedBatch(binding.draw_indexed(first, 3, 1, 0, 0, 1), binding.draw_indexed(second, 3, 1, 0, 0, 1));
        second.copy(changed);
        ExpectTriangleColor(render([&](auto& stream) {
            EXPECT_THROW(stream << batch, std::logic_error);
            EXPECT_FALSE(first.resource_use().state->busy());
            EXPECT_FALSE(positions.resource_use().state->busy());
            stream << binding.draw_indexed(second, 3, 1, 0, 0, 1);
        }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, UnchangedAndFailedIndexCopiesKeepCapturedCommandsUsable) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        auto command = typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 0, 0, 1);
        ExpectTriangleColor(render([&](auto& stream) { stream << command; }));
        EXPECT_THROW(indices.copy(std::span(values).first(2)), std::invalid_argument);
        ExpectTriangleColor(render([&](auto& stream) { stream << command; }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, NestedIndexViewsUseRelativeFirstIndexAndBatchRebindsOffsets) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 6> values{99, 99, 1, 2, 3, 99};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        const auto outer = indices.view(1, 4), nested = outer.view(1, 3);
        EXPECT_EQ(outer.byte_offset(), 2u);
        EXPECT_EQ(nested.byte_offset(), 4u);
        EXPECT_EQ(nested.size(), 3u);
        EXPECT_EQ(nested.bounds(0, 3), (std::pair<std::uint32_t, std::uint32_t>{1, 3}));
        const auto binding = typed.bind_vertices(positions, colors);
        auto batch = t::TypedBatch(binding.draw_indexed(outer, 3, 1, 1, -1, 1),
                                   binding.draw_indexed(nested, 3, 1, 0, -1, 1));
        EXPECT_EQ(batch.statistics().index_binds, 2u);
        ExpectTriangleColor(render([&](auto& stream) { stream << batch; }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, DeviceLocalIndexViewsPreserveUint32ValuesAndVertexOffset) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint32_t, 5> values{99, 65536, 65537, 65538, 99};
        auto manager = make_manager();
        t::TypedIndexBuffer<std::uint32_t, vsl::MemoryProperty::DEVICE_LOCAL> indices(device, manager, values);
        const auto view = indices.view(1, 3);
        EXPECT_EQ(view.byte_offset(), sizeof(std::uint32_t));
        EXPECT_EQ(view.bounds(0, 3), (std::pair<std::uint32_t, std::uint32_t>{65536, 65538}));
        ExpectTriangleColor(render([&](auto& stream) {
            stream << typed.bind_vertices(positions, colors).draw_indexed(view, 3, 1, 0, -65536, 1);
        }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, ViewsRejectEmptyEscapingAndOverflowingRanges) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 5> values{99, 1, 2, 3, 99};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        EXPECT_THROW(indices.view(0, 0), std::out_of_range);
        EXPECT_THROW(indices.view(values.size(), 1), std::out_of_range);
        EXPECT_THROW(indices.view(2, std::numeric_limits<std::size_t>::max()), std::out_of_range);
        const auto view = indices.view(1, 3);
        EXPECT_THROW(view.view(1, 3), std::out_of_range);
        EXPECT_THROW(view.view(0, 0), std::out_of_range);
        EXPECT_THROW(view.bounds(1, 3), std::out_of_range);
        const auto binding = typed.bind_vertices(positions, colors);
        EXPECT_THROW(binding.draw_indexed(view, 4), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(view, 3, 1, 1), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(view, 3, 1, 0, 0, 1), std::out_of_range);
        EXPECT_NO_THROW(binding.draw_indexed(view, 0, 0, view.size()));
        ExpectTriangleColor(render([&](auto& stream) { stream << binding.draw_indexed(view, 3, 1, 0, -1, 1); }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, ViewsShareUpdateRevisionsAndBlockParentWritesDuringDrawing) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 5> values{99, 0, 1, 2, 99}, changed{99, 2, 1, 0, 99};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        const auto view = indices.view(1, 3);
        const auto binding = typed.bind_vertices(positions, colors);
        auto old = binding.draw_indexed(view, 3, 1, 0, 0, 1);
        indices.copy(changed);
        EXPECT_EQ(view.bounds(0, 1), (std::pair<std::uint32_t, std::uint32_t>{2, 2}));
        ExpectTriangleColor(render([&](auto& stream) {
            EXPECT_THROW(stream << old, std::logic_error);
            stream << binding.draw_indexed(view, 3, 1, 0, 0, 1);
            EXPECT_TRUE(indices.resource_use().state->busy());
            EXPECT_THROW(indices.copy(values), std::logic_error);
        }));
        EXPECT_NO_THROW(indices.copy(values));
        EXPECT_EQ(view.bounds(0, 1), (std::pair<std::uint32_t, std::uint32_t>{0, 0}));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, ViewsAndCommandsOutliveSourceWrappers) {
        std::weak_ptr<vsl::impl::BufferImpl> owner;
        auto command = [&] {
            auto typed = make_typed();
            t::TypedVertexBuffer<Position> positions(device, POSITIONS);
            t::TypedVertexBuffer<Color> colors(device, COLORS);
            const auto view = [&] {
                const std::array<std::uint16_t, 5> values{99, 0, 1, 2, 99};
                t::TypedIndexBuffer<std::uint16_t> indices(device, values);
                owner = indices.backend().data_;
                return indices.view(1, 3);
            }();
            EXPECT_FALSE(owner.expired());
            return typed.bind_vertices(positions, colors).draw_indexed(view, 3, 1, 0, 0, 1);
        }();
        std::optional owned(std::move(command));
        ExpectTriangleColor(render([&](auto& stream) {
            stream << *owned;
            owned.reset();
            EXPECT_FALSE(owner.expired());
        }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, CommandsKeepIndexAndVertexOwnersAfterAllWrappersAreDestroyed) {
        std::weak_ptr<vsl::impl::BufferImpl> owner;
        auto command = [&] {
            auto typed = make_typed();
            t::TypedVertexBuffer<Position> positions(device, POSITIONS);
            t::TypedVertexBuffer<Color> colors(device, COLORS);
            const std::array<std::uint16_t, 3> values{0, 1, 2};
            t::TypedIndexBuffer<std::uint16_t> indices(device, values);
            owner = indices.backend().data_;
            return typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 0, 0, 1);
        }();
        std::optional owned(std::move(command));
        ExpectTriangleColor(render([&](auto& stream) { stream << *owned; owned.reset(); EXPECT_FALSE(owner.expired()); }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, IndexedDrawKeepsDescriptorAndPushContracts) {
        struct Tint {
            float rgba[4];
        };
        using TintLayout = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
        using Set = t::DescriptorSet<1, t::UniformBinding<4, TintLayout, vsl::ShaderFlag::VERTEX>>;
        using Resources = t::ResourceSchema<Set>;
        using Push = t::PushSchema<t::PushRange<TintLayout, vsl::ShaderFlag::VERTEX, 16>>;
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path("typed_push_resources.vert.spv"));
        t::TypedGraphicsPipeline<t::TypedVertexInput<>, Resources, Push> typed(device, *pass,
                                                                               pl::ShaderGroup("indexed resources", {shader, *fragment}), pl::ColorBlend{}, pl::InputAssembly{},
                                                                               pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        const Tint bias{{.125f, .25f, .375f, 0}};
        t::TypedUniformBuffer<TintLayout> uniform(device, bias);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Set::Layout> set(device, manager);
        set.write<4>(uniform);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << typed.bind_vertices().push_constants(Push::Values(bias)).bind_resources(set).draw_indexed(indices, 3);
        }));
    }
} // namespace
