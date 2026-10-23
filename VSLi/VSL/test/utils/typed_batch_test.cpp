#include <VSL/utils/typed_batch.hpp>
#include "support/render_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    using Empty = t::TypedVertexInput<>;
    struct Position {
        float xy[2];
    };
    struct Color {
        float rgb[3];
    };
    using Positions = t::VertexRecord<Position, VSL_VERTEX_MEMBER(Position, xy)>;
    using Colors = t::VertexRecord<Color, VSL_VERTEX_MEMBER(Color, rgb)>;
    using Input = t::TypedVertexInput<Positions::Binding<0>, Colors::Binding<1, t::VertexTiming::NEXT_VERTEX, 1>>;
    const std::array<Position, 3> POSITIONS{{{{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
    class TypedBatchVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        template<typename Layout = Empty, typename Resources = t::ResourceSchema<>, typename Pushes = t::PushSchema<>>
        t::TypedGraphicsPipeline<Layout, Resources, Pushes> make_typed(const char* path = "triangle.vert.spv");
    };
    template<typename Layout, typename Resources, typename Pushes>
    t::TypedGraphicsPipeline<Layout, Resources, Pushes> TypedBatchVulkanIntegrationTest::make_typed(const char* path) {
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path(path));
        return {device, *pass, pl::ShaderGroup("batch", {shader, *fragment}), pl::ColorBlend{}, pl::InputAssembly{},
                pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description)};
    }
    TEST_F(TypedBatchVulkanIntegrationTest, RepeatedDrawsBindOnceAndBatchReuseStartsFromFreshState) {
        auto typed = make_typed();
        auto other = make_typed("typed_batch_red.vert.spv");
        auto batch = t::TypedBatch(typed.bind_vertices().draw(3), typed.bind_vertices().draw(3));
        EXPECT_EQ(batch.statistics().pipeline_binds, 1u);
        EXPECT_EQ(batch.statistics().draws, 2u);
        for (int iteration = 0; iteration < 2; ++iteration)
            ExpectTriangleColor(render([&](auto& stream) { stream << other.backend() << batch; }));
    }
    TEST_F(TypedBatchVulkanIntegrationTest, DistinctPipelineInstancesOfTheSameTypeAreBoundAndTheLastIsReported) {
        auto first = make_typed("typed_batch_red.vert.spv");
        auto last = make_typed();
        auto batch = t::TypedBatch(first.bind_vertices().draw(3), last.bind_vertices().draw(3));
        EXPECT_EQ(batch.statistics().pipeline_binds, 2u);
        EXPECT_EQ(batch.get_pipeline().data_, last.backend().data_);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << batch;
            EXPECT_EQ(stream.pipeline->data_, last.backend().data_);
            stream << vsl::command::Draw(3);
        }));
    }
    TEST_F(TypedBatchVulkanIntegrationTest, VertexBufferIdentityViewOffsetsAndIndexTypesArePartOfThePlan) {
        auto typed = make_typed<Input>("vertices.vert.spv");
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        const std::array<Color, 6> values{{{{1, 0, 0}}, {{1, 0, 0}}, {{1, 0, 0}}, {{.25f, .5f, .75f}}, {{.25f, .5f, .75f}}, {{.25f, .5f, .75f}}}};
        t::TypedVertexBuffer<Color> colors(device, values);
        t::TypedVertexBuffer<Color> same_type_different_buffer(device, std::span(values).first(3));
        const std::array<std::uint16_t, 3> shorts{0, 1, 2};
        const std::array<std::uint32_t, 3> longs{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices16(device, shorts);
        t::TypedIndexBuffer<std::uint32_t> indices32(device, longs);
        auto first = typed.bind_vertices(positions, colors.view(0, 3));
        auto other = typed.bind_vertices(positions, same_type_different_buffer);
        auto last = typed.bind_vertices(positions, colors.view(3, 3));
        auto batch = t::TypedBatch(first.draw(3), other.draw(3), last.draw_indexed(indices16, 3), last.draw_indexed(indices16, 3), last.draw_indexed(indices32, 3));
        EXPECT_EQ(batch.statistics().pipeline_binds, 1u);
        EXPECT_EQ(batch.statistics().vertex_binds, 4u);
        EXPECT_EQ(batch.statistics().index_binds, 2u);
        EXPECT_EQ(batch.statistics().draws, 5u);
        ExpectTriangleColor(render([&](auto& stream) { stream << batch; }));
    }
    TEST_F(TypedBatchVulkanIntegrationTest, DescriptorIdentityAndEveryPushSnapshotArePreserved) {
        struct Tint {
            float rgba[4];
        };
        using Layout = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
        using Set = t::DescriptorSet<1, t::UniformBinding<4, Layout, vsl::ShaderFlag::VERTEX>>;
        using Resources = t::ResourceSchema<Set>;
        using Push = t::PushSchema<t::PushRange<Layout, vsl::ShaderFlag::VERTEX, 16>>;
        auto typed = make_typed<Empty, Resources, Push>("typed_push_resources.vert.spv");
        const Tint red{{1, 0, 0, 0}}, half{{.125f, .25f, .375f, 0}}, zero{};
        t::TypedUniformBuffer<Layout> first_buffer(device, red), last_buffer(device, half);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Set::Layout> first(device, manager), last(device, manager);
        first.write<4>(first_buffer);
        last.write<4>(last_buffer);
        auto batch = t::TypedBatch(
            typed.bind_vertices().bind_resources(first).push_constants(Push::Values(zero)).draw(3),
            typed.bind_vertices().bind_resources(last).push_constants(Push::Values(zero)).draw(3),
            typed.bind_vertices().bind_resources(last).push_constants(Push::Values(half)).draw(3));
        EXPECT_EQ(batch.statistics().pipeline_binds, 1u);
        EXPECT_EQ(batch.statistics().descriptor_binds, 2u);
        ExpectTriangleColor(render([&](auto& stream) { stream << batch; }));
    }
    TEST_F(TypedBatchVulkanIntegrationTest, HeterogeneousCommandsRebindDescriptorsAcrossPipelineChangesAndKeepOwners) {
        struct Tint {
            float rgba[4];
        };
        using Layout = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
        using Set = t::DescriptorSet<1, t::UniformBinding<4, Layout, vsl::ShaderFlag::VERTEX>>;
        using Resources = t::ResourceSchema<Set>;
        using Push = t::PushSchema<t::PushRange<Layout, vsl::ShaderFlag::VERTEX, 16>>;
        std::weak_ptr<vsl::impl::BufferImpl> owner;
        auto batch = [&] {
            auto plain = make_typed();
            auto typed = make_typed<Empty, Resources, Push>("typed_push_resources.vert.spv");
            auto alternate = make_typed<Empty, Resources, Push>("typed_push_resources.vert.spv");
            const Tint half{{.125f, .25f, .375f, 0}};
            t::TypedUniformBuffer<Layout> uniform(device, half);
            owner = uniform.backend().data_;
            vsl::GraphicResourceManager manager(device);
            t::TypedDescriptorSet<Set::Layout> set(device, manager);
            set.write<4>(uniform);
            auto first = typed.bind_vertices().bind_resources(set).push_constants(Push::Values(half)).draw(3);
            auto last = alternate.bind_vertices().bind_resources(set).push_constants(Push::Values(half)).draw(3);
            return t::TypedBatch(plain.bind_vertices().draw(3), std::move(first), std::move(last));
        }();
        EXPECT_EQ(batch.statistics().pipeline_binds, 3u);
        EXPECT_EQ(batch.statistics().descriptor_binds, 2u);
        std::optional owned(std::move(batch));
        ExpectTriangleColor(render([&](auto& stream) { stream << *owned; owned.reset(); EXPECT_FALSE(owner.expired()); }));
    }
    TEST_F(TypedBatchVulkanIntegrationTest, ComputeBatchDeduplicatesBindingsAndPreservesDistinctPushValues) {
        struct PushData {
            std::uint32_t first, value;
        };
        using Layout = t::BlockRecord<PushData, VSL_BLOCK_MEMBER(PushData, first), VSL_BLOCK_MEMBER(PushData, value)>;
        using Push = t::PushSchema<t::PushRange<Layout, vsl::ShaderFlag::COMPUTE>>;
        using Set = t::DescriptorSet<0, t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using Resources = t::ResourceSchema<Set>;
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_batch.comp.spv"));
        t::TypedComputePipeline<Resources, Push> typed(device, vsl::pipeline_layout::ShaderGroup("compute batch", {shader}));
        std::array<std::uint32_t, 8> zero{};
        t::TypedStorageBuffer<std::uint32_t> output(device, zero);
        vsl::GraphicResourceManager resources(device);
        t::TypedDescriptorSet<Set::Layout> set(device, resources);
        set.write<0>(output);
        auto binding = typed.bind_resources(set);
        auto batch = t::TypedBatch(binding.push_constants(Push::Values(PushData{0, 10})).dispatch(4), binding.push_constants(Push::Values(PushData{4, 20})).dispatch(4));
        EXPECT_EQ(batch.statistics().pipeline_binds, 1u);
        EXPECT_EQ(batch.statistics().descriptor_binds, 1u);
        EXPECT_EQ(batch.statistics().dispatches, 2u);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << batch;
            auto native = manager.get_buffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            native.data_->command_buffers[native.get_current_buffer_idx()].pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = output.backend().data();
        const auto* actual = static_cast<const std::uint32_t*>(mapped.data);
        for (size_t i = 0; i < 4; ++i) {
            EXPECT_EQ(actual[i], 10u + i);
            EXPECT_EQ(actual[4 + i], 20u + i);
        }
    }
} // namespace
