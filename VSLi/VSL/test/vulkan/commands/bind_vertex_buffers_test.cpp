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
    expect_triangle_color(render([&](auto &stream) {
        binding.invoke(stream.manager.getPool(), stream.manager.getBuffer(), stream.manager);
        stream << vsl::command::Draw{3};
    }));
}
} // namespace
#include "support/compile_probe.hpp"
namespace {
TEST(BindVertexBufferCompilationTest, DISABLED_NonConstLvalueCanBeSentThroughPhaseStream) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
void probe(vsl::DefaultPhaseStream& stream,vsl::command::BindVertexBuffer& command){stream<<command;}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
} // namespace
