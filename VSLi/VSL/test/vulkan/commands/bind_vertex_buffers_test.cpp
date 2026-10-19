#include "support/render_fixture.hpp"
namespace {
    using BindVertexBufferVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(BindVertexBufferVulkanIntegrationTest, BindsSeparatePositionAndColorBuffers) {
        configure_pipeline(true);
        std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
        std::array<float, 9> colors{.25f, .5f, .75f, .25f, .5f, .75f, .25f, .5f, .75f};
        vsl::StagingBuffer<vsl::MemoryType::VertexBuffer> position(device, positions);
        vsl::StagingBuffer<vsl::MemoryType::VertexBuffer> color_buffer(device, colors);
        vsl::command::BindVertexBuffer binding(position);
        binding.add(color_buffer);
        ASSERT_EQ(binding.buffers.size(), 2u);
        EXPECT_EQ(binding.buffers[0], &position);
        EXPECT_EQ(binding.buffers[1], &color_buffer);
        expect_triangle_color(render([&](auto& stream) {
            stream << binding << vsl::command::Draw{3};
        }));
    }
} // namespace
namespace {
    TEST(BindVertexBufferTest, EmptyCommandsCanBeCopiedAssignedAndMoved) {
        vsl::command::BindVertexBuffer empty;
        vsl::command::BindVertexBuffer copy(empty);
        const auto const_copy = copy;
        vsl::command::BindVertexBuffer assigned;
        assigned = const_copy;
        auto moved = std::move(assigned);
        EXPECT_TRUE(empty.buffers.empty());
        EXPECT_TRUE(copy.buffers.empty());
        EXPECT_TRUE(moved.buffers.empty());
    }
    TEST_F(BindVertexBufferVulkanIntegrationTest, CopiesKeepIndependentListsAndTheOriginalBufferReferences) {
        configure_pipeline(true);
        std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
        std::array<float, 9> colors{.25f, .5f, .75f, .25f, .5f, .75f, .25f, .5f, .75f};
        vsl::StagingBuffer<vsl::MemoryType::VertexBuffer> position(device, positions), color(device, colors);
        vsl::command::BindVertexBuffer original(position, color);
        vsl::command::BindVertexBuffer copied(original);
        original.buffers.clear();
        ASSERT_EQ(copied.buffers.size(), 2u);
        EXPECT_EQ(copied.buffers[0], &position);
        EXPECT_EQ(copied.buffers[1], &color);
        vsl::command::BindVertexBuffer assigned;
        assigned = copied;
        auto moved = std::move(assigned);
        expect_triangle_color(render([&](auto& stream) {
            stream << moved << vsl::command::Draw{3};
        }));
    }
} // namespace

TEST(BindVertexBufferTest, VariadicConstructionAcceptsOnlyMutableVertexBuffers) {
    using Command = vsl::command::BindVertexBuffer;
    using Vertex = vsl::StagingBuffer<vsl::MemoryType::VertexBuffer>;
    using Index = vsl::StagingBuffer<vsl::MemoryType::IndexBuffer>;
    static_assert(std::is_constructible_v<Command, Vertex&>);
    static_assert(std::is_constructible_v<Command, Vertex&, Vertex&>);
    static_assert(!std::is_constructible_v<Command, const Vertex&>);
    static_assert(!std::is_constructible_v<Command, volatile Vertex&>);
    static_assert(!std::is_constructible_v<Command, Vertex&&>);
    static_assert(!std::is_constructible_v<Command, Index&>);
    static_assert(!std::is_constructible_v<Command, int&>);
    static_assert(!std::is_constructible_v<Command, vsl::BufferAccessor&>);
    EXPECT_TRUE(std::is_copy_constructible_v<Command>);
    EXPECT_TRUE(std::is_move_constructible_v<Command>);
}

namespace {
    class DrawResourceLifetimeVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(DrawResourceLifetimeVulkanIntegrationTest, DrawSurvivesDroppingBuffersAndPipelineBeforeSubmission) {
        configure_pipeline(true);
        std::weak_ptr<vsl::_impl::Buffer_impl> weak_vertex, weak_color, weak_index;
        std::weak_ptr<vsl::_impl::Pipeline_impl> weak_pipeline = pipeline->_data;
        expect_triangle_color(render([&](auto& stream) {
            const std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
            const std::array<float, 9> colors{.25f, .5f, .75f, .25f, .5f, .75f, .25f, .5f, .75f};
            const std::array<std::uint32_t, 3> indices{0, 1, 2};
            vsl::StagingBuffer<vsl::MemoryType::VertexBuffer> position(device, positions), color(device, colors);
            vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
            weak_vertex = position._data;
            weak_color = color._data;
            weak_index = index._data;
            stream << vsl::command::BindVertexBuffer(position, color) << vsl::command::BindIndexBuffer(index)
                   << vsl::command::DrawIndexed{3};
            pipeline.reset();
            layout.reset();
        }));
        EXPECT_TRUE(weak_vertex.expired());
        EXPECT_TRUE(weak_color.expired());
        EXPECT_TRUE(weak_index.expired());
        EXPECT_TRUE(weak_pipeline.expired());
    }
} // namespace
