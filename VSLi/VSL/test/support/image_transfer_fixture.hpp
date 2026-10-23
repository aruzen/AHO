#pragma once
#include "vulkan_fixture.hpp"
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>
#include <array>
namespace vsl_test {
    class ImageTransferVulkanIntegrationTest : public VulkanIntegrationTest {
    protected:
        using TransferImage =
            vsl::Image<vsl::ImageType::TRANSFER_SRC | vsl::ImageType::TRANSFER_DST | vsl::ImageType::SAMPLED>;
        void transition(vsl::CommandManager manager, vsl::ImageAccessor image, vk::ImageLayout old_layout,
                        vk::ImageLayout new_layout, vk::AccessFlags source, vk::AccessFlags destination);
        std::array<std::uint32_t, 4> round_trip(bool typed, bool array_image);
    };

    inline void ImageTransferVulkanIntegrationTest::transition(vsl::CommandManager manager, vsl::ImageAccessor image, vk::ImageLayout old_layout,
                                                               vk::ImageLayout new_layout, vk::AccessFlags source, vk::AccessFlags destination) {
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcAccessMask = source;
        barrier.dstAccessMask = destination;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image.data_->image;
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, image.data_->count};
        auto buffer = manager.get_buffer();
        buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {},
            barrier);
    }

    inline std::array<std::uint32_t, 4> ImageTransferVulkanIntegrationTest::round_trip(bool typed, bool array_image) {
        const std::array<std::uint32_t, 4> expected{0x12345678, 0x90abcdef, 0, 0xffffffff};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT, array_image ? 2 : 1);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_SOURCE> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
                       vk::AccessFlagBits::eTransferWrite);
            if (array_image)
                phase << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::GENERAL, 1);
            else
                phase << vsl::command::CopyBufferToImage(image, &source, vsl::ImageLayout::GENERAL);
            transition(manager, image, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                       vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
            if (array_image) {
                vk::BufferImageCopy region{};
                region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 1, 1};
                region.imageExtent = {2, 2, 1};
                auto buffer = manager.get_buffer();
                buffer.data_->command_buffers[buffer.get_current_buffer_idx()].copyImageToBuffer(
                    image.data_->image, vk::ImageLayout::eGeneral, destination.data_->buffer, region);
            } else if (typed)
                phase << vsl::command::CopyImageToBuffer(destination, image);
            else
                phase << vsl::command::CopyImageToBuffer(&destination, image);
        }
        device.data_->device.waitIdle();
        std::array<std::uint32_t, 4> actual{};
        auto mapped = destination.data(sizeof(actual));
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        return actual;
    }
} // namespace vsl_test
