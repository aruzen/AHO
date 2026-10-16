#include "support/image_transfer_fixture.hpp"
namespace {
using CopyImageToBufferVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
TEST_F(CopyImageToBufferVulkanIntegrationTest, TypedAndAccessorOverloadsPreserveEveryTexel) {
    const std::array<std::uint32_t, 4> expected{0x12345678, 0x90abcdef, 0, 0xffffffff};
    EXPECT_EQ(round_trip(false, false), expected);
    EXPECT_EQ(round_trip(true, false), expected);
}
} // namespace
