#include "support/render_fixture.hpp"
namespace {
using DrawInstanceVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
TEST(DrawInstanceTest, ConstructorsRetainBothCounts) {
    const vsl::command::DrawInstance explicit_counts(3, 2), implicit_count(4);
    EXPECT_EQ(explicit_counts.index_size, 3u);
    EXPECT_EQ(explicit_counts.instance_size, 2u);
    EXPECT_EQ(implicit_count.index_size, 0u);
    EXPECT_EQ(implicit_count.instance_size, 4u);
}
TEST_F(DrawInstanceVulkanIntegrationTest, DrawsTwoIndexedInstances) {
    std::array<std::uint32_t, 3> indices{0, 1, 2};
    vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
    expect_triangle_color(render([&](auto &stream) {
        auto buffer = stream.manager.getBuffer();
        buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindIndexBuffer(index._data->buffer, 0,
                                                                                   vk::IndexType::eUint32);
        stream << vsl::command::DrawInstance{3, 2};
    }));
}
} // namespace
