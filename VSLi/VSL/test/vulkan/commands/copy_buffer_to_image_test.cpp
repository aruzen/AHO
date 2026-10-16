#include "support/image_transfer_fixture.hpp"
namespace {
using CopyBufferToImageVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
TEST_F(CopyBufferToImageVulkanIntegrationTest, CopiesEveryTexelAndSelectedArrayLayer) {
    const std::array<std::uint32_t, 4> expected{0x12345678, 0x90abcdef, 0, 0xffffffff};
    EXPECT_EQ(round_trip(false, false), expected);
    EXPECT_EQ(round_trip(false, true), expected);
}
TEST_F(CopyBufferToImageVulkanIntegrationTest, TypedConstructorRetainsImageBufferAndDeclaredLayout) {
    TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
    vsl::StagingBuffer<vsl::MemoryType::TransferSource> source(device, std::size_t{16});
    vsl::command::CopyBufferToImage command(image, source);
    EXPECT_EQ(command.image._data, image._data);
    EXPECT_EQ(command.buffer, &source);
    EXPECT_EQ(command.layout, vsl::ImageLayout::Undefined);
    // Undefined is not a valid transfer layout; invocation of this specialization is excluded.
}
} // namespace
#include "support/compile_probe.hpp"
namespace {
TEST(ImageTransferCompilationTest, DISABLED_TypedArrayCopyDelegatesToItsOwnConstructor) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
void probe(vsl::Image<vsl::ImageType::TransferDst>& image,
           vsl::StagingBuffer<vsl::MemoryType::TransferSource>& buffer){
    vsl::command::CopyBufferToArrayImage command(image,buffer,1);
}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
} // namespace
