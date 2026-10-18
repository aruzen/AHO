#include "support/image_transfer_fixture.hpp"
#include <VSL/vulkan/post_definition.hpp>

namespace {
using ImageCopyDefinitionVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
TEST_F(ImageCopyDefinitionVulkanIntegrationTest, CopyByBufferReturnsTrueAndUploadsEveryTexel) {
    const std::array<std::uint32_t, 4> expected{17, 0, 0xffffffffu, 73};
    TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
    vsl::StagingBuffer<vsl::MemoryType::TransferSource> source(device, expected);
    vsl::StagingBuffer<vsl::MemoryType::TransferDestination> destination(device, sizeof(expected));
    auto manager = make_manager();
    ASSERT_TRUE(image.copyByBuffer(manager, source));
    device._data->device.waitIdle();
    {
        auto phase = manager.startPhase<vsl::ComputePhase>();
        transition(manager, image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
                   vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
        phase << vsl::command::CopyImageToBuffer(&destination, image);
        vk::MemoryBarrier host_visibility{};
        host_visibility.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        host_visibility.dstAccessMask = vk::AccessFlagBits::eHostRead;
        auto commands = manager.getBuffer();
        commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, host_visibility, {}, {});

    }
    device._data->device.waitIdle();
    auto mapped = destination.data(sizeof(expected));
    EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
}
} // namespace
