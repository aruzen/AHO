#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <source_location>
#include <algorithm>

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::string traceinfo) : VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                                                                                                                             "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                                                                                                                             traceinfo) {}

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::source_location sourece) : VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                                                                                                                                    "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                                                                                                                                    sourece) {}

std::uint32_t VSL_NAMESPACE::_impl::helper::findMemoryType(std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl> device, std::uint32_t typeFilter,
                                                           vk::MemoryPropertyFlags properties) {
    if (not device->parentDevice->memProps.has_value())
        device->parentDevice->makeMemProps();
    auto& memProperties = device->parentDevice->memProps.value();
    for (size_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw VSL_NAMESPACE::exceptions::RuntimeException("MemoryTypeNotFound", "No compatible memory type satisfies the requested properties");
}

bool VSL_NAMESPACE::BufferAccessor::copyByBuffer(CommandManager commandManager, BufferAccessor* buf) {
    auto commandBuf = commandManager.makeExclusiveBuffer();

    auto vkCommandBuf = commandBuf._data->commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo;
    vkCommandBuf.begin(beginInfo);

    vk::BufferCopy copyRegion;
    copyRegion.size = buf->_data->requirementSize;
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

    return true;
}

bool VSL_NAMESPACE::BufferAccessor::copyByImage(CommandManager manager, const VSL_NAMESPACE::ImageAccessor& image) {
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::CopyImageToBuffer(this, image);
    return true;
}

size_t VSL_NAMESPACE::BufferAccessor::size() {
    return _data->requirementSize;
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
    bufferInfo.usage = vk::BufferUsageFlags{(unsigned int)memType};
    bufferInfo.sharingMode = (vk::SharingMode)sharingMode;
    const std::uint32_t queueFamilies[] = {device._data->graphicsFamily, device._data->presentFamily};
    if (sharingMode == SharingMode::Concurrent) {
        if (queueFamilies[0] == queueFamilies[1]) {
            bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        } else {
            bufferInfo.queueFamilyIndexCount = 2;
            bufferInfo.pQueueFamilyIndices = queueFamilies;
        }
    }

    data->requirementSize = size;
    data->buffer = data->device->device.createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements = data->device->device.getBufferMemoryRequirements(data->buffer);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VSL_NAMESPACE::_impl::helper::findMemoryType(data->device, memRequirements.memoryTypeBits,
                                                                             vk::MemoryPropertyFlags{(unsigned int)memProperty});

    data->deviceMem = data->device->device.allocateMemory(allocInfo);
    data->allocatedSize = allocInfo.allocationSize;

    data->device->device.bindBufferMemory(data->buffer, data->deviceMem, 0);
    return data;
}

namespace {
vk::MappedMemoryRange atom_aligned_range(const vsl::BufferAccessor::LocalBufferHolder &holder) {
    const auto &data = holder.parent->_data;
    if (holder.offset > data->requirementSize || holder.size == 0 ||
        holder.size > data->requirementSize - holder.offset)
        throw vsl::exceptions::RuntimeException("buffer", "mapped range is outside requested storage");
    const auto &physical = data->device->parentDevice;
    if (!physical->props)
        physical->makeProps();
    const auto atom = physical->props->limits.nonCoherentAtomSize;
    const vk::DeviceSize start = holder.offset - holder.offset % atom;
    vk::DeviceSize end = holder.offset + holder.size;
    if (const auto remainder = end % atom; remainder != 0)
        end += std::min<vk::DeviceSize>(atom - remainder, data->allocatedSize - end);
    vk::MappedMemoryRange range{};
    range.memory = data->deviceMem;
    range.offset = start;
    range.size = end - start;
    return range;
}
} // namespace

void
VSL_NAMESPACE::BufferAccessor::FlushBuffer(LocalBufferHolder &holder, MemoryType memType, MemoryProperty memProperty,
                                                SharingMode sharingMode) {
    if (not contain(memProperty, MemoryProperty::HostVisible)) {
        throw VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException(std::source_location::current());
    }
    if (contain(memProperty, MemoryProperty::HostCoherent))
        return;

    const auto range = atom_aligned_range(holder);
    holder.parent->_data->device->device.flushMappedMemoryRanges(range);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::BufferAccessor::
    GetData(VSL_NAMESPACE::BufferAccessor* data, std::optional<size_t> size, size_t offset, MemoryType memType,
            MemoryProperty memProperty, SharingMode sharingMode) {
    if (not contain(memProperty, MemoryProperty::HostVisible)) {
        throw VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException(std::source_location::current());
    }


    const auto requested = data->_data->requirementSize;
    if (offset > requested)
        throw vsl::exceptions::RuntimeException("buffer", "mapped offset is outside requested storage");
    const auto bytes = size.value_or(requested - offset);
    if (bytes == 0 || bytes > requested - offset)
        throw vsl::exceptions::RuntimeException("buffer", "mapped range is outside requested storage");
    VSL_NAMESPACE::BufferAccessor::LocalBufferHolder holder(nullptr, bytes, offset);
    if (contain(memProperty, MemoryProperty::HostCoherent)) {
        holder.data = data->_data->device->device.mapMemory(data->_data->deviceMem, offset, bytes);
        holder.parent = data;
    } else {
        auto *mapped = data->_data->device->device.mapMemory(data->_data->deviceMem, 0, VK_WHOLE_SIZE);
        holder.data = static_cast<unsigned char *>(mapped) + offset;
        holder.parent = data;
        data->_data->device->device.invalidateMappedMemoryRanges(atom_aligned_range(holder));
    }

    return std::move(holder);
}

bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copyByImage(VSL_NAMESPACE::CommandManager commandManager, const VSL_NAMESPACE::ImageAccessor& image) {
    return parent->copyByImage(std::move(commandManager), image);
}

bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copyByBuffer(VSL_NAMESPACE::CommandManager commandManager, VSL_NAMESPACE::BufferAccessor* buf) {
    return parent->copyByBuffer(std::move(commandManager), buf);
    ;
}

VSL_NAMESPACE::_impl::Buffer_impl::~Buffer_impl() {
    device->device.destroyBuffer(buffer);
    device->device.freeMemory(deviceMem);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::~LocalBufferHolder() {
    if (parent)
        parent->_data->device->device.unmapMemory(parent->_data->deviceMem);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::LocalBufferHolder(VSL_NAMESPACE::BufferAccessor::LocalBufferHolder&& other) {
    this->parent = other.parent;
    this->data = other.data;
    this->size = other.size;
    this->offset = other.offset;

    other.parent = VK_NULL_HANDLE;
    other.data = VK_NULL_HANDLE;
    other.size = 0;
    other.offset = 0;
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::LocalBufferHolder(VSL_NAMESPACE::BufferAccessor* parent, std::size_t size,
                                                                    std::size_t offset, void* data)
    : parent(parent), size(size), offset(offset), data(data) {}

bool VSL_NAMESPACE::helper::copy_with_shift_offset(void* data, size_t maxSize, size_t& offset) {
    return true;
}
