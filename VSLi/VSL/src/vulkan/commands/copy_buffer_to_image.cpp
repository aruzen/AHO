//
// Created by morimoto_hibiki on 2025/10/22.
//

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>

#include <utility>

vsl::command::CopyBufferToImage::CopyBufferToImage(vsl::ImageAccessor image, vsl::BufferAccessor *buffer,
                                                   vsl::ImageLayout layout)
        : image(std::move(image)), buffer(buffer), layout(layout) {}

void vsl::command::CopyBufferToImage::invoke(
        vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;  // tightly packed
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{image._data->width, image._data->height, 1};

    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].copyBufferToImage(
            this->buffer->_data->buffer,
            this->image._data->image,
            (vk::ImageLayout) layout,
            region
    );
}
