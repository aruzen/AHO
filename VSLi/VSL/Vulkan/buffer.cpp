#ifdef _MSC_VER
#include "pch.h"
#endif
#include "_pimpls.h"

#include "buffer.h"
#include <source_location>

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::string traceinfo) :
	VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException", "This buffer's memory is not host-visible. Mapping is not allowed.", traceinfo) {}

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::source_location sourece) :
	VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException", "This buffer's memory is not host-visible. Mapping is not allowed.", sourece) {}

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

bool VSL_NAMESPACE::BufferAccessor::copy(BufferAccessor* buf)
{
	return copyByBuffer(buf);
}

bool VSL_NAMESPACE::BufferAccessor::copyByBuffer(BufferAccessor* buf) {
	_data->buffer;

	if (not _data->commandManager.has_value()) {
		// TODO throw error
	}
	auto commandManager = CommandManager{ _data->commandManager.value() };
	auto commandBuf = commandManager.makeExclusiveBuffer();

	auto vkCommandBuf = commandBuf._data->commandBuffers[0];

	vk::CommandBufferBeginInfo beginInfo;
	vkCommandBuf.begin(beginInfo);

	vk::BufferCopy copyRegion;
	copyRegion.size = buf->size();
	/* TODO offset
	copyRegion.srcOffset = buf->
	*/
	vkCommandBuf.copyBuffer(buf->_data->buffer, this->_data->buffer, { copyRegion });

	vkCommandBuf.end();

	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkCommandBuf;

	commandManager._data->graphicsQueue.submit({ submitInfo });
	commandManager._data->graphicsQueue.waitIdle();

	return false;
}

size_t VSL_NAMESPACE::BufferAccessor::size()
{
	return _data->allocatedSize;
}


void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush() {
	parent->flush(*this);
}

std::shared_ptr<VSL_NAMESPACE::_impl::Buffer_impl> VSL_NAMESPACE::BufferAccessor::MakeBuffer(LogicalDeviceAccessor device, size_t size, MemoryType memType, MemoryProperty memProperty, SharingMode sharingMode)
{
	auto data = std::make_shared<_impl::Buffer_impl>();
	data->device = device._data;

	vk::BufferCreateInfo bufferInfo;
	bufferInfo.size = size;
	bufferInfo.usage = vk::BufferUsageFlags{ (unsigned int)memType };
	bufferInfo.sharingMode = (vk::SharingMode)sharingMode;

	data->buffer = device._data->device.createBuffer(bufferInfo);

	vk::MemoryRequirements memRequirements = device._data->device.getBufferMemoryRequirements(data->buffer);

	vk::MemoryAllocateInfo allocInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(device, memRequirements.memoryTypeBits, vk::MemoryPropertyFlags{ (unsigned int)memProperty });

	data->deviceMem = device._data->device.allocateMemory(allocInfo);
	data->allocatedSize = allocInfo.allocationSize;

	device._data->device.bindBufferMemory(data->buffer, data->deviceMem, 0);

	return data;
}

void VSL_NAMESPACE::BufferAccessor::FlushBuffer(LocalBufferHolder holder, MemoryType memType, MemoryProperty memProperty, SharingMode sharingMode) {
	if (not contain(memProperty, MemoryProperty::HostVisible)) {
		throw VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException(std::source_location::current());
	}

	vk::MappedMemoryRange range;
	range.memory = holder.parent->_data->deviceMem;
	range.offset = holder.offset;
	range.size = holder.size;

	holder.parent->_data->device->device.flushMappedMemoryRanges(range);
}


VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::BufferAccessor::
GetData(VSL_NAMESPACE::BufferAccessor* data, std::optional<size_t> size, size_t offset, MemoryType memType, MemoryProperty memProperty, SharingMode sharingMode)
{
	if (not contain(memProperty, MemoryProperty::HostVisible)) {
		throw VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException(std::source_location::current());
	}

	VSL_NAMESPACE::BufferAccessor::LocalBufferHolder holder = { data, size ? size.value() : data->_data->allocatedSize, offset };
	holder.data =
		data->_data->device->device.mapMemory(data->_data->deviceMem, offset, size ? size.value() : data->_data->allocatedSize);
	return holder;
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
