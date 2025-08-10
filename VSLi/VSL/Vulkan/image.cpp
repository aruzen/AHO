//
// Created by morimoto_hibiki on 2025/07/31.
//
#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"

#include "_pimpls.h"
#include "../exceptions.h"

#include "image.hpp"

// TODO 統一する
extern std::uint32_t findMemoryType(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t typeFilter, vk::MemoryPropertyFlags properties);

vsl::Image::Image(vsl::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, data_format::___Format format) {
    _data = std::make_shared<_impl::Image_impl>();
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
    imageInfo.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    _data->image = device._data->device.createImage(imageInfo);

    vk::MemoryRequirements memRequirements = device._data->device.getImageMemoryRequirements(_data->image);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(device, memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

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
}
