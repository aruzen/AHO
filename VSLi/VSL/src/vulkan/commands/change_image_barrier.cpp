#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include "../image_format.hpp"

vsl::command::ChangeImageBarrier::ChangeImageBarrier(vsl::ImageAccessor image,
                                                     ImageLayout oldLayout,
                                                     ImageLayout newLayout)
    : image(image), oldLayout(oldLayout), newLayout(newLayout) {}

vsl::command::ChangeImageBarrier::ChangeImageBarrier(vsl::ImageAccessor image, ImageLayout oldLayout,
                                                     ImageLayout newLayout, std::uint32_t baseLayer,
                                                     std::uint32_t layerCount)
    : image(image), oldLayout(oldLayout), newLayout(newLayout), baseLayer(baseLayer), layerCount(layerCount) {}

void vsl::command::ChangeImageBarrier::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer,
                                              vsl::CommandManager manager) {
    if (!image._data || !image._data->image || !buffer._data || !buffer._data->commandPool || !manager._data ||
        image._data->device != manager._data->device ||
        buffer._data->commandPool->device != image._data->device)
        throw std::invalid_argument("Image barrier requires an image and command buffer on the same device");
    if (baseLayer >= image._data->count)
        throw std::out_of_range("Image barrier base layer exceeds image");
    const auto layers = layerCount ? layerCount : image._data->count - baseLayer;
    if (layers > image._data->count - baseLayer)
        throw std::out_of_range("Image barrier layer range exceeds image");
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = (vk::ImageLayout)oldLayout;
    barrier.newLayout = (vk::ImageLayout)newLayout;
    barrier.image = image._data->image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = _impl::helper::imageAspects(image._data->format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = baseLayer;
    barrier.subresourceRange.layerCount = layers;

    vk::PipelineStageFlags sourceStage, destinationStage;
    if (oldLayout == ImageLayout::Undefined && newLayout == ImageLayout::TransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == ImageLayout::Undefined && newLayout == ImageLayout::ColorAttachmentOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else if (oldLayout == ImageLayout::TransferDstOptimal && newLayout == ImageLayout::ShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == ImageLayout::ColorAttachmentOptimal && newLayout == ImageLayout::TransferSrcOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == ImageLayout::TransferSrcOptimal && newLayout == ImageLayout::ColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else if (newLayout == ImageLayout::TransferSrcOptimal &&
               (oldLayout == ImageLayout::General || oldLayout == ImageLayout::ShaderReadOnlyOptimal ||
                oldLayout == ImageLayout::TransferDstOptimal || oldLayout == ImageLayout::Present ||
                oldLayout == ImageLayout::TransferSrcOptimal)) {
        barrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        sourceStage = vk::PipelineStageFlagBits::eAllCommands;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == ImageLayout::TransferSrcOptimal &&
               (newLayout == ImageLayout::General || newLayout == ImageLayout::ShaderReadOnlyOptimal ||
                newLayout == ImageLayout::TransferDstOptimal || newLayout == ImageLayout::Present)) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eAllCommands;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    buffer._data->commandBuffers.at(buffer.getCurrentBufferIdx()).pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);
}
