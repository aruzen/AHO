//
// Created by morimoto_hibiki on 2025/07/31.
//
#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif
#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/image.hpp>

#include <utility>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>


std::shared_ptr<vsl::_impl::Image_impl>
vsl::ImageAccessor::MakeImage(vsl::ImageType type, vsl::MemoryProperty prop, vsl::ImageLayout layout,
                              vsl::LogicalDeviceAccessor device, std::uint32_t width,
                              std::uint32_t height, vsl::data_format::___Format format) {
    auto _data = std::make_shared<_impl::Image_impl>();
    _data->device = device._data;
    _data->height = height;
    _data->width = width;

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = (vk::Format)format.type();
    imageInfo.extent = vk::Extent3D{ width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = (vk::ImageUsageFlagBits) type;
    imageInfo.initialLayout = (vk::ImageLayout) layout;

    _data->image = device._data->device.createImage(imageInfo);

    vk::MemoryRequirements memRequirements = device._data->device.getImageMemoryRequirements(_data->image);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vsl::_impl::helper::findMemoryType(device._data, memRequirements.memoryTypeBits,
                                                                   (vk::MemoryPropertyFlagBits) prop);

    _data->memory = device._data->device.allocateMemory(allocInfo);

    device._data->device.bindImageMemory(_data->image, _data->memory, 0);

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = _data->image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = (vk::Format)format.type();;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    _data->view = device._data->device.createImageView(viewInfo);
    return _data;
}

void vsl::ImageAccessor::TransformLayout(vsl::CommandManager manager, std::shared_ptr<_impl::Image_impl> data,
                                         vsl::ImageLayout oldLayout, vsl::ImageLayout newLayout) {
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::ChangeImageBarrier(ImageAccessor{std::move(data)}, oldLayout, newLayout);
}

bool vsl::ImageAccessor::copyByBuffer(CommandManager manager, BufferAccessor *buf, vsl::ImageLayout layout) {
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::CopyBufferToImage(*this, buf, layout);
    return true;
}

vsl::_impl::Image_impl::~Image_impl() {
    device->device.destroy(view);
    device->device.destroy(image);
    device->device.free(memory);
}