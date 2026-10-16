#include "support/render_fixture.hpp"
namespace {
using DrawIndexedVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
TEST(DrawIndexedTest, ConstructorsAndManipulatorRetainIndexCount) {
    EXPECT_EQ(vsl::command::DrawIndexed{}.size, 0u);
    EXPECT_EQ(vsl::command::DrawIndexed{3}.size, 3u);
    vsl::command::DrawIndexedManip draw;
    draw.setVertexSize(3);
    EXPECT_EQ(draw.size, 3u);
    draw.setVertexSize(std::nullopt);
    EXPECT_EQ(draw.size, 3u);
}
TEST_F(DrawIndexedVulkanIntegrationTest, DrawsIndexedTriangle) {
    std::array<std::uint32_t, 3> indices{0, 1, 2};
    vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
    expect_triangle_color(render([&](auto &stream) {
        // BindIndexBuffer has a dangling pointer (I05); isolate DrawIndexed with a raw binding.
        auto buffer = stream.manager.getBuffer();
        buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].bindIndexBuffer(index._data->buffer, 0,
                                                                                   vk::IndexType::eUint32);
        stream << vsl::command::DrawIndexed{3};
    }));
}
} // namespace
