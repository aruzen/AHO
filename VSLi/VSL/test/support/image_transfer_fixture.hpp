#pragma once
#include "vulkan_fixture.hpp"
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>
#include <array>
namespace vsl_test {
class ImageTransferVulkanIntegrationTest : public VulkanIntegrationTest {
  protected:
    using TransferImage =
        vsl::Image<vsl::ImageType::TransferSrc | vsl::ImageType::TransferDst | vsl::ImageType::Sampled>;
    void transition(vsl::CommandManager manager, vsl::ImageAccessor image, vk::ImageLayout old_layout,
                    vk::ImageLayout new_layout, vk::AccessFlags source, vk::AccessFlags destination) {
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcAccessMask = source;
        barrier.dstAccessMask = destination;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image._data->image;
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, image._data->count};
        auto buffer = manager.getBuffer();
        buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {},
            barrier);
    }
    std::array<std::uint32_t, 4> round_trip(bool typed, bool array_image) {
        const std::array<std::uint32_t, 4> expected{0x12345678, 0x90abcdef, 0, 0xffffffff};
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt, array_image ? 2 : 1);
        vsl::StagingBuffer<vsl::MemoryType::TransferSource> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> destination(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
                       vk::AccessFlagBits::eTransferWrite);
            if (array_image)
                phase << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::General, 1);
            else
                phase << vsl::command::CopyBufferToImage(image, &source, vsl::ImageLayout::General);
            transition(manager, image, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                       vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
            if (array_image) {
                vk::BufferImageCopy region{};
                region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 1, 1};
                region.imageExtent = {2, 2, 1};
                auto buffer = manager.getBuffer();
                buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].copyImageToBuffer(
                    image._data->image, vk::ImageLayout::eGeneral, destination._data->buffer, region);
            } else if (typed)
                phase << vsl::command::CopyImageToBuffer(destination, image);
            else
                phase << vsl::command::CopyImageToBuffer(&destination, image);
        }
        device._data->device.waitIdle();
        std::array<std::uint32_t, 4> actual{};
        auto mapped = destination.data(sizeof(actual));
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        return actual;
    }
};
} // namespace vsl_test
