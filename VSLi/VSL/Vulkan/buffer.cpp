#ifdef _MSC_VER
#include "pch.h"
#endif
#include "_pimpls.h"

#include "buffer.h"

std::uint32_t findMemoryType(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t typeFilter, vk::MemoryPropertyFlags properties) {

    if (not device._data->parentDevice->memProps.has_value())
        device._data->parentDevice->makeMemProps();
    auto& memProperties = device._data->parentDevice->memProps.value();
    for (size_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
}

size_t VSL_NAMESPACE::BufferAccessor::size()
{
    return _data->allocatedSize;
}

/*
void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush(){
    parent->_data->device->device.flushMappedMemoryRanges()
}
*/

std::shared_ptr<VSL_NAMESPACE::_impl::Buffer_impl> VSL_NAMESPACE::BufferAccessor::make_buffer(LogicalDeviceAccessor device, size_t size, MemoryType memType, MemoryProperty memProperty, SharingMode sharingMode)
{
	auto data = std::make_shared<_impl::Buffer_impl>();
    data->device = device._data;

    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    /*
    bufferInfo.usage = vk::BufferUsageFlags{ (unsigned int)memType };
    bufferInfo.sharingMode = (vk::SharingMode)sharingMode;
    */

    data->buffer = device._data->device.createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements = device._data->device.getBufferMemoryRequirements(data->buffer);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    // allocInfo.memoryTypeIndex = findMemoryType(device, memRequirements.memoryTypeBits, vk::MemoryPropertyFlags{ (unsigned int)memProperty });
    allocInfo.memoryTypeIndex = findMemoryType(device, memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    data->deviceMem = device._data->device.allocateMemory(allocInfo);
    data->allocatedSize = allocInfo.allocationSize;

    device._data->device.bindBufferMemory(data->buffer, data->deviceMem, 0);

	return data;
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::BufferAccessor::data(std::optional<size_t> size, size_t offset)
{
    return { this, _data->device->device.mapMemory(_data->deviceMem, offset, size ? size.value() : _data->allocatedSize) };
}

VSL_NAMESPACE::_impl::Buffer_impl::~Buffer_impl() {
    device->device.destroyBuffer(buffer);
    device->device.freeMemory(deviceMem);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::~LocalBufferHolder()
{
    parent->_data->device->device.unmapMemory(parent->_data->deviceMem);
}

bool VSL_NAMESPACE::helper::copy_with_shift_offset(void* data, size_t maxSize, size_t& offset)
{
    return true;
}
