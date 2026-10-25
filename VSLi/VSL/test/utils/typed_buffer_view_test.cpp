#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_compute.hpp>
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
    using Input = t::TypedVertexInput<Positions::Binding<0>, Colors::Binding<1, t::VertexTiming::NEXT_VERTEX, 1>>;
    class TypedViewVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        template<typename Layout, std::uint32_t subpass = 0>
        t::TypedGraphicsPipeline<Layout, t::ResourceSchema<>, t::PushSchema<>, subpass> make_typed(vsl::RenderPassAccessor target);
    };
    template<typename Layout, std::uint32_t subpass>
    t::TypedGraphicsPipeline<Layout, t::ResourceSchema<>, t::PushSchema<>, subpass> TypedViewVulkanIntegrationTest::make_typed(vsl::RenderPassAccessor target) {
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path(Layout::BINDING_COUNT ? "vertices.vert.spv" : "triangle.vert.spv"));
        return {device, target, pl::ShaderGroup("view", {shader, *fragment}), pl::ColorBlend{}, pl::InputAssembly{},
                pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description)};
    }
    TEST(TypedViewTest, OffsetArithmeticRejectsEmptyOutOfBoundsAndOverflow) {
        EXPECT_THROW(t::detail::checked_view_offset(0, 0, 0, 3, 8), std::out_of_range);
        EXPECT_THROW(t::detail::checked_view_offset(0, 2, 2, 3, 8), std::out_of_range);
        EXPECT_THROW(t::detail::checked_view_offset(SIZE_MAX, 1, 1, 2, 8), std::overflow_error);
    }
    TEST_F(TypedViewVulkanIntegrationTest, VertexSubviewsKeepTheirOffsetsCapacityAndOwners) {
        const std::array<Position, 5> positions{{{{99, 99}}, {{99, 99}}, {{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
        const std::array<Color, 5> colors{{{{1, 0, 0}}, {{1, 0, 0}}, {{.25f, .5f, .75f}}, {{.25f, .5f, .75f}}, {{.25f, .5f, .75f}}}};
        auto position_view = [&] { t::TypedVertexBuffer<Position> buffer(device, positions); return buffer.view(1, 4).view(1, 3); }();
        auto color_view = [&] { t::TypedVertexBuffer<Color> buffer(device, colors); return buffer.view(2, 3); }();
        EXPECT_EQ(position_view.byte_offset(), 2 * sizeof(Position));
        auto typed = make_typed<Input>(*pass);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed.bind_vertices(position_view, color_view).draw(3); }));
        const std::array<std::uint16_t, 3> indices{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> index(device, indices);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed.bind_vertices(position_view, color_view).draw_indexed(index, 3); }));
        EXPECT_THROW(typed.bind_vertices(position_view, color_view).draw(4), std::out_of_range);
        EXPECT_THROW(position_view.view(3, 1), std::out_of_range);
    }
    TEST_F(TypedViewVulkanIntegrationTest, StorageDescriptorViewWritesOnlyTheSelectedBufferRange) {
        using Set = t::DescriptorSet<0, t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using Resources = t::ResourceSchema<Set>;
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("fill.comp.spv"));
        using Push = t::PushSchema<t::PushRange<std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        t::TypedComputePipeline<Resources, Push> typed(device, vsl::pipeline_layout::ShaderGroup("storage view", {shader}));
        const auto alignment = device.data_->parent_device->device.getProperties().limits.minStorageBufferOffsetAlignment;
        const size_t first = std::max<size_t>(1, alignment / sizeof(std::uint32_t));
        std::vector<std::uint32_t> zero(first + 4, 777);
        t::TypedStorageBuffer<std::uint32_t> storage(device, zero);
        vsl::GraphicResourceManager resources(device);
        t::TypedDescriptorSet<Set::Layout> set(device, resources);
        auto view = storage.view(first, 4).view(0, 4);
        EXPECT_EQ(view.byte_offset(), first * sizeof(std::uint32_t));
        set.write<0>(view);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << typed.bind_resources(set).push_constants(Push::Values(std::uint32_t(41))).dispatch(4);
            auto native = manager.get_buffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            native.data_->command_buffers[native.get_current_buffer_idx()].pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = storage.backend().data();
        const auto* actual = static_cast<const std::uint32_t*>(mapped.data);
        for (size_t i = 0; i < first; ++i)
            EXPECT_EQ(actual[i], 777u);
        for (size_t i = 0; i < 4; ++i)
            EXPECT_EQ(actual[first + i], 41u + i);
        EXPECT_THROW(storage.view(first, 5), std::out_of_range);
    }
    TEST_F(TypedViewVulkanIntegrationTest, MisalignedUniformViewsAreRejectedAndAlignedViewsRetainTheirOwners) {
        using Set = t::DescriptorLayout<t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::VERTEX>>;
        const auto alignment = device.data_->parent_device->device.getProperties().limits.minUniformBufferOffsetAlignment;
        const size_t first = std::max<size_t>(1, alignment / sizeof(std::uint32_t));
        std::vector<std::uint32_t> values(first + 1, 7);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Set> set(device, manager);
        std::weak_ptr<vsl::impl::BufferImpl> owner;
        {
            t::TypedUniformBuffer<std::uint32_t> buffer(device, values);
            owner = buffer.backend().data_;
            if (alignment > sizeof(std::uint32_t))
                EXPECT_THROW(set.write<0>(buffer.view(1, 1)), std::invalid_argument);
            set.write<0>(buffer.view(first, 1));
        }
        EXPECT_FALSE(owner.expired());
        EXPECT_FALSE(has_validation_errors());
    }
    TEST_F(TypedViewVulkanIntegrationTest, TypedPipelineSelectsTheSecondSubpassAndRejectsInvalidIndices) {
        vsl::RenderPassDescription desc;
        desc.add_color({.format = vsl::data_format::UNSIGNED_NORMALIZED8_RGBA, .final_layout = vsl::ImageLayout::GENERAL});
        desc.subpasses.push_back(desc.subpasses.front());
        desc.dependencies = {{.source = vsl::EXTERNAL_SUBPASS, .destination = 0}, {.source = 0, .destination = 1}, {.source = 1, .destination = vsl::EXTERNAL_SUBPASS}};
        vsl::RenderPass target(device, desc);
        vsl::FrameBuffer frame(target, WIDTH, HEIGHT, std::vector<vsl::ImageAccessor>{*color});
        auto first = make_typed<t::TypedVertexInput<>, 0>(target);
        auto second = make_typed<t::TypedVertexInput<>, 1>(target);
        static_assert(decltype(second)::SUBPASS == 1);
        EXPECT_THROW((make_typed<t::TypedVertexInput<>, 2>(target)), std::out_of_range);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto stream = phase << vsl::command::RenderPassBegin(target, frame);
            stream << vsl::Viewport(description) << vsl::Scissor(description) << first.bind_vertices().draw(3)
                   << vsl::command::NextSubpass{} << second.bind_vertices().draw(3) << vsl::command::RenderPassEnd{};
        }
        device.data_->device.waitIdle();
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> readback(device, WIDTH * HEIGHT * 4);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::CopyImageToBuffer(&readback, *color);
        }
        device.data_->device.waitIdle();
        auto mapped = readback.data();
        std::array<std::uint8_t, 4> pixel{};
        std::memcpy(pixel.data(), static_cast<const std::byte*>(mapped.data) + ((HEIGHT / 2) * WIDTH + WIDTH / 2) * 4, 4);
        ExpectTriangleColor(pixel);
    }
} // namespace
