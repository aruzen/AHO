#include "support/vulkan_fixture.hpp"
namespace {
using BindIndexBufferVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(BindIndexBufferVulkanIntegrationTest, DISABLED_RetainsCallerBufferInsteadOfTemporaryParameter) {
    std::array<std::uint32_t, 3> indices{0, 1, 2};
    vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> buffer(device, indices);
    vsl::command::BindIndexBuffer command(buffer, 4);
    EXPECT_EQ(command.offset, 4u);
    EXPECT_EQ(command.buffer, &buffer); // Do not dereference the dangling pointer (I05).
}
} // namespace
