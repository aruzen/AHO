#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <source_location>
#include <stdexcept>
#include <cstring>
#include <utility>

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

static bool copyBuffer(const std::shared_ptr<VSL_NAMESPACE::_impl::Buffer_impl>& destination, VSL_NAMESPACE::CommandManager commandManager, VSL_NAMESPACE::BufferAccessor* buf) {
    auto commandBuf = commandManager.makeExclusiveBuffer();

    auto vkCommandBuf = commandBuf._data->commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo;
    vkCommandBuf.begin(beginInfo);

    vk::BufferCopy copyRegion;
    copyRegion.size = buf->_data->requirementSize;
    /* TODO offset
    copyRegion.srcOffset = buf->
    */
    vkCommandBuf.copyBuffer(buf->_data->buffer, destination->buffer, {copyRegion});

    vkCommandBuf.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCommandBuf;

    commandManager._data->graphicsQueue.submit({submitInfo});
    commandManager._data->graphicsQueue.waitIdle();

    return true;
}

bool VSL_NAMESPACE::BufferAccessor::copyByBuffer(CommandManager manager, BufferAccessor* source) {
    return copyBuffer(_data, std::move(manager), source);
}

bool VSL_NAMESPACE::BufferAccessor::copyByImage(CommandManager manager, const VSL_NAMESPACE::ImageAccessor& image) {
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::CopyImageToBuffer(this, image);
    return true;
}

size_t VSL_NAMESPACE::BufferAccessor::size() {
    return _data->requirementSize;
}

std::size_t VSL_NAMESPACE::BufferAccessor::requestedSize() const {
    return _data ? _data->requirementSize : 0;
}

void VSL_NAMESPACE::BufferAccessor::flushCopied(LocalBufferHolder& holder, std::size_t bytes) {
    if (!bytes)
        return;
    holder.size = bytes;
    flush(holder);
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::BufferAccessor::getDevice() const {
    if (!_data || !_data->device)
        throw std::logic_error("Buffer is empty");
    return LogicalDeviceAccessor{_data->device};
}
VSL_NAMESPACE::MemoryType VSL_NAMESPACE::BufferAccessor::getUsage() const {
    return _data ? _data->usage : MemoryType::None;
}
VSL_NAMESPACE::MemoryProperty VSL_NAMESPACE::BufferAccessor::getMemoryProperties() const {
    return _data ? _data->properties : MemoryProperty::None;
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush() {
    flush(size);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::synchronize(std::size_t bytes, std::size_t relativeOffset, bool flush) const {
    if (!owner_ || !data || !owner_->mapped)
        throw std::logic_error("Buffer mapping is empty");
    if (relativeOffset > size || bytes > size - relativeOffset ||
        offset > owner_->requirementSize || size > owner_->requirementSize - offset)
        throw std::out_of_range("Buffer mapping range");
    if (!bytes || contain(owner_->properties, MemoryProperty::HostCoherent))
        return;
    const auto atom = owner_->atomSize;
    const vk::DeviceSize first = offset + relativeOffset;
    const vk::DeviceSize begin = first - first % atom;
    const vk::DeviceSize end = first + bytes;
    const auto padding = (atom - end % atom) % atom;
    const auto length = owner_->allocatedSize - end < padding ? VK_WHOLE_SIZE : end + padding - begin;
    const vk::MappedMemoryRange range{.memory = owner_->deviceMem, .offset = begin, .size = length};
    if (flush)
        owner_->device->device.flushMappedMemoryRanges(range);
    else
        owner_->device->device.invalidateMappedMemoryRanges(range);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush(std::size_t bytes, std::size_t offset) const {
    synchronize(bytes, offset, true);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::invalidate(std::size_t bytes, std::size_t offset) const {
    synchronize(bytes, offset, false);
}

std::span<const std::byte> VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::read(std::size_t bytes, std::size_t offset) const {
    invalidate(bytes, offset);
    return {static_cast<const std::byte*>(data) + offset, bytes};
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::write(std::span<const std::byte> bytes, std::size_t relativeOffset) const {
    if (!owner_ || !data)
        throw std::logic_error("Buffer mapping is empty");
    if (relativeOffset > size || bytes.size() > size - relativeOffset ||
        offset > owner_->requirementSize || size > owner_->requirementSize - offset)
        throw std::out_of_range("Buffer mapping write range");
    if (!bytes.empty())
        std::memmove(static_cast<std::byte*>(data) + relativeOffset, bytes.data(), bytes.size());
    flush(bytes.size(), relativeOffset);
}

std::shared_ptr<VSL_NAMESPACE::_impl::Buffer_impl>
VSL_NAMESPACE::BufferAccessor::MakeBuffer(LogicalDeviceAccessor device, size_t size, MemoryType memType,
                                          MemoryProperty memProperty, SharingMode sharingMode) {
    if (!device._data || !device._data->device || !size || memType == MemoryType::None)
        throw std::invalid_argument("Buffer requires a device, positive size and usage");
    auto data = std::make_shared<_impl::Buffer_impl>();
    data->device = device._data;
    data->usage = memType;
    data->requirementSize = size;
    vk::BufferCreateInfo info{.size = size, .usage = vk::BufferUsageFlags{static_cast<unsigned int>(memType)}, .sharingMode = static_cast<vk::SharingMode>(sharingMode)};
    const std::uint32_t families[] = {device._data->graphicsFamily, device._data->presentFamily};
    if (sharingMode == SharingMode::Concurrent) {
        if (families[0] == families[1])
            info.sharingMode = vk::SharingMode::eExclusive;
        else {
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = families;
        }
    }
    data->buffer = data->device->device.createBuffer(info);
    const auto requirements = data->device->device.getBufferMemoryRequirements(data->buffer);
    const auto properties = device._data->parentDevice->device.getMemoryProperties();
    const auto required = vk::MemoryPropertyFlags{static_cast<unsigned int>(memProperty)};
    std::optional<std::uint32_t> selected;
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        const auto flags = properties.memoryTypes[i].propertyFlags;
        if (!(requirements.memoryTypeBits & (1u << i)) || (flags & required) != required)
            continue;
        if (!selected)
            selected = i;
        if (contain(memProperty, MemoryProperty::HostVisible) && (flags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
            selected = i;
            break;
        }
    }
    if (!selected)
        throw exceptions::RuntimeException("MemoryTypeNotFound", "No memory type satisfies the buffer requirements");
    data->properties = static_cast<MemoryProperty>(static_cast<unsigned int>(properties.memoryTypes[*selected].propertyFlags));
    data->allocatedSize = requirements.size;
    data->atomSize = device._data->parentDevice->device.getProperties().limits.nonCoherentAtomSize;
    data->deviceMem = data->device->device.allocateMemory({.allocationSize = requirements.size, .memoryTypeIndex = *selected});
    data->device->device.bindBufferMemory(data->buffer, data->deviceMem, 0);
    return data;
}

void VSL_NAMESPACE::BufferAccessor::FlushBuffer(LocalBufferHolder& holder, MemoryType, MemoryProperty memProperty, SharingMode) {
    if (!contain(memProperty, MemoryProperty::HostVisible))
        throw exceptions::MemoryNotHostVisibleException(std::source_location::current());
    holder.flush();
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::BufferAccessor::GetData(
    BufferAccessor* buffer, std::optional<size_t> bytes, size_t offset, MemoryType, MemoryProperty memProperty, SharingMode) {
    if (!contain(memProperty, MemoryProperty::HostVisible))
        throw exceptions::MemoryNotHostVisibleException(std::source_location::current());
    if (!buffer || !buffer->_data)
        throw std::logic_error("Buffer is empty");
    const auto& storage = buffer->_data;
    if (offset > storage->requirementSize)
        throw exceptions::RuntimeException("buffer", "mapped offset is outside requested storage");
    const auto length = bytes.value_or(storage->requirementSize - offset);
    if (length > storage->requirementSize - offset)
        throw exceptions::RuntimeException("buffer", "mapped range is outside requested storage");
    if (!storage->mapped)
        storage->mapped = storage->device->device.mapMemory(storage->deviceMem, 0, VK_WHOLE_SIZE);
    LocalBufferHolder holder(buffer, length, offset, static_cast<std::byte*>(storage->mapped) + offset);
    holder.invalidate(length);
    return holder;
}

bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copyByImage(CommandManager manager, const ImageAccessor& image) {
    if (!owner_)
        throw std::logic_error("Buffer mapping is empty");
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::CopyImageToBuffer(owner_, image, ImageLayout::General);
    return true;
}

bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copyByBuffer(CommandManager manager, BufferAccessor* buffer) {
    if (!owner_)
        throw std::logic_error("Buffer mapping is empty");
    return copyBuffer(owner_, std::move(manager), buffer);
}

VSL_NAMESPACE::_impl::Buffer_impl::~Buffer_impl() {
    if (mapped)
        device->device.unmapMemory(deviceMem);
    device->device.destroyBuffer(buffer);
    device->device.freeMemory(deviceMem);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::~LocalBufferHolder() = default;

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::LocalBufferHolder(LocalBufferHolder&& other)
    : parent(std::exchange(other.parent, nullptr)), size(std::exchange(other.size, 0)), offset(std::exchange(other.offset, 0)),
      data(std::exchange(other.data, nullptr)), owner_(std::move(other.owner_)) {}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::LocalBufferHolder(BufferAccessor* parent, std::size_t size, std::size_t offset, void* data)
    : parent(parent), size(size), offset(offset), data(data), owner_(parent ? parent->_data : nullptr) {}

bool VSL_NAMESPACE::helper::copy_with_shift_offset(void* data, size_t maxSize, size_t& offset) {
    return true;
}
