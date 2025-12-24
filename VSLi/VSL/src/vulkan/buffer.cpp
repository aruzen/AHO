#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <source_location>

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::string traceinfo) :
        VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                    "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                    traceinfo) {}

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::source_location sourece) :
        VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                    "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                    sourece) {}

std::uint32_t vsl::_impl::helper::findMemoryType(std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl> device, std::uint32_t typeFilter,
                             vk::MemoryPropertyFlags properties) {
    if (not device->parentDevice->memProps.has_value())
        device->parentDevice->makeMemProps();
    auto &memProperties = device->parentDevice->memProps.value();
    for (size_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

bool VSL_NAMESPACE::BufferAccessor::copyByBuffer(CommandManager commandManager, BufferAccessor *buf) {
    auto commandBuf = commandManager.makeExclusiveBuffer();

    auto vkCommandBuf = commandBuf._data->commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo;
    vkCommandBuf.begin(beginInfo);

    vk::BufferCopy copyRegion;
    copyRegion.size = buf->size();
    /* TODO offset
    copyRegion.srcOffset = buf->
    */
    vkCommandBuf.copyBuffer(buf->_data->buffer, this->_data->buffer, {copyRegion});

    vkCommandBuf.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCommandBuf;

    commandManager._data->graphicsQueue.submit({submitInfo});
    commandManager._data->graphicsQueue.waitIdle();

    return false;
}

bool vsl::BufferAccessor::copyByImage(CommandManager manager, const vsl::ImageAccessor& image) {
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::CopyImageToBuffer(this, image);
    return true;
}

size_t VSL_NAMESPACE::BufferAccessor::size() {
    return _data->allocatedSize;
}


void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush() {
    parent->flush(*this);
}

std::shared_ptr<VSL_NAMESPACE::_impl::Buffer_impl>
VSL_NAMESPACE::BufferAccessor::MakeBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, MemoryType memType,
                                          MemoryProperty memProperty, SharingMode sharingMode) {
    auto data = std::make_shared<_impl::Buffer_impl>();
    data->device = device._data;

    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = vk::BufferUsageFlags{(unsigned int) memType};
    bufferInfo.sharingMode = (vk::SharingMode) sharingMode;

    data->buffer = data->device->device.createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements = data->device->device.getBufferMemoryRequirements(data->buffer);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vsl::_impl::helper::findMemoryType(data->device, memRequirements.memoryTypeBits,
                                               vk::MemoryPropertyFlags{(unsigned int) memProperty});

    data->deviceMem = data->device->device.allocateMemory(allocInfo);
    data->allocatedSize = allocInfo.allocationSize;

    data->device->device.bindBufferMemory(data->buffer, data->deviceMem, 0);
    return data;
}

void
VSL_NAMESPACE::BufferAccessor::FlushBuffer(LocalBufferHolder holder, MemoryType memType, MemoryProperty memProperty,
                                           SharingMode sharingMode) {
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
GetData(VSL_NAMESPACE::BufferAccessor *data, std::optional<size_t> size, size_t offset, MemoryType memType,
        MemoryProperty memProperty, SharingMode sharingMode) {
    if (not contain(memProperty, MemoryProperty::HostVisible)) {
        throw VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException(std::source_location::current());
    }

    VSL_NAMESPACE::BufferAccessor::LocalBufferHolder holder = {data, size ? size.value() : data->_data->allocatedSize,
                                                               offset};
    holder.data = data->_data->device->device.mapMemory(data->_data->deviceMem, offset,
                                                  size ? size.value() : data->_data->allocatedSize);

    vk::MappedMemoryRange mappedRange;
    mappedRange.memory = data->_data->deviceMem;
    mappedRange.offset = offset;
    mappedRange.size = size ? size.value() : data->_data->allocatedSize;  // またはコピー範囲のサイズ

    data->_data->device->device.invalidateMappedMemoryRanges({mappedRange});

    return holder;
}

bool vsl::BufferAccessor::LocalBufferHolder::copyByImage(vsl::CommandManager commandManager, const vsl::ImageAccessor &image) {
    return parent->copyByImage(std::move(commandManager), image);
}

bool vsl::BufferAccessor::LocalBufferHolder::copyByBuffer(vsl::CommandManager commandManager, vsl::BufferAccessor *buf) {
    return parent->copyByBuffer(std::move(commandManager), buf);;
}


VSL_NAMESPACE::_impl::Buffer_impl::~Buffer_impl() {
    device->device.destroyBuffer(buffer);
    device->device.freeMemory(deviceMem);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::~LocalBufferHolder() {
    parent->_data->device->device.unmapMemory(parent->_data->deviceMem);
}

bool VSL_NAMESPACE::helper::copy_with_shift_offset(void *data, size_t maxSize, size_t &offset) {
    return true;
}
