#include "support/render_fixture.hpp"
#include <type_traits>
namespace {
    using BindIndexBufferVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(BindIndexBufferVulkanIntegrationTest, RetainsCallerBufferInsteadOfTemporaryParameter) {
        std::array<std::uint32_t, 3> indices{0, 1, 2};
        vsl::StagingBuffer<vsl::MemoryType::INDEX_BUFFER> buffer(device, indices);
        vsl::command::BindIndexBuffer command(buffer, 4);
        EXPECT_EQ(command.offset, 4u);
        ASSERT_EQ(command.buffer, &buffer);
        EXPECT_EQ(command.buffer->data_, buffer.data_);
        const auto copy = command;
        EXPECT_EQ(copy.buffer, &buffer);
        EXPECT_EQ(copy.offset, 4u);
    }
    TEST(BindIndexBufferTest, BorrowsAnLvalueAndRejectsTemporaryBuffers) {
        using Buffer = vsl::StagingBuffer<vsl::MemoryType::INDEX_BUFFER>;
        using Command = vsl::command::BindIndexBuffer;
        static_assert(std::is_constructible_v<Command, Buffer&>);
        static_assert(std::is_constructible_v<Command, Buffer&, std::uint32_t>);
        static_assert(!std::is_constructible_v<Command, Buffer&&>);
        static_assert(!std::is_constructible_v<Command, const Buffer&>);
        SUCCEED();
    }
    using BindIndexRenderVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(BindIndexRenderVulkanIntegrationTest, DrawsUsingTheCallerBufferAndDefaultOffset) {
        std::array<std::uint32_t, 3> indices{0, 1, 2};
        vsl::StagingBuffer<vsl::MemoryType::INDEX_BUFFER> buffer(device, indices);
        vsl::command::BindIndexBuffer command(buffer);
        EXPECT_EQ(command.offset, 0u);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << command << vsl::command::DrawIndexed{3};
        }));
    }
    TEST_F(BindIndexRenderVulkanIntegrationTest, ExplicitByteOffsetSkipsTheLeadingIndex) {
        // Without the offset, indices 0,0,1 form a degenerate triangle.
        std::array<std::uint32_t, 4> indices{0, 0, 1, 2};
        vsl::StagingBuffer<vsl::MemoryType::INDEX_BUFFER> buffer(device, indices);
        vsl::command::BindIndexBuffer command(buffer, sizeof(std::uint32_t));
        const auto copied_command = command;
        ExpectTriangleColor(render([&](auto& stream) {
            stream << copied_command << vsl::command::DrawIndexed{3};
        }));
    }
} // namespace
