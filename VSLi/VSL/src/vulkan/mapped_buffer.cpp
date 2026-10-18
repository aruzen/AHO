#include <VSL/vulkan/mapped_buffer.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace vsl {
    MappedBuffer::MappedBuffer(LogicalDeviceAccessor owner, std::size_t bytes, MemoryType usage) {
        if (!owner._data || !owner._data->device || !bytes || usage == MemoryType::None)
            throw std::invalid_argument("MappedBuffer requires a device, positive size and usage");
        auto data = std::make_shared<_impl::MappedBuffer_impl>();
        data->usage = usage;
        data->buffer = std::make_shared<_impl::Buffer_impl>();
        auto &buffer = *data->buffer;
        buffer.device = owner._data;
        buffer.requirementSize = bytes;
        auto device = owner._data->device;
        buffer.buffer = device.createBuffer({.size = bytes,
                                             .usage = vk::BufferUsageFlags(static_cast<std::uint32_t>(usage)),
                                             .sharingMode = vk::SharingMode::eExclusive});
        const auto requirements = device.getBufferMemoryRequirements(buffer.buffer);
        const auto properties = owner._data->parentDevice->device.getMemoryProperties();
        std::optional<std::uint32_t> memoryType;
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            const auto flags = properties.memoryTypes[i].propertyFlags;
            if (!(requirements.memoryTypeBits & (1u << i)) || !(flags & vk::MemoryPropertyFlagBits::eHostVisible))
                continue;
            memoryType = i;
            data->coherent = bool(flags & vk::MemoryPropertyFlagBits::eHostCoherent);
            if (data->coherent)
                break;
        }
        if (!memoryType)
            throw std::runtime_error("No host-visible memory type");
        buffer.allocatedSize = requirements.size;
        buffer.deviceMem = device.allocateMemory({.allocationSize = requirements.size, .memoryTypeIndex = *memoryType});
        device.bindBufferMemory(buffer.buffer, buffer.deviceMem, 0);
        data->mapped = device.mapMemory(buffer.deviceMem, 0, VK_WHOLE_SIZE);
        if (!data->coherent)
            data->atomSize = owner._data->parentDevice->device.getProperties().limits.nonCoherentAtomSize;
        _data = std::move(data);
    }
    _impl::MappedBuffer_impl::~MappedBuffer_impl() {
        if (mapped)
            buffer->device->device.unmapMemory(buffer->deviceMem);
    }
    std::size_t MappedBuffer::size() const { return _data ? _data->buffer->requirementSize : 0; }
    std::span<const std::byte> MappedBuffer::read(std::size_t bytes, std::size_t offset) const {
        if (!_data)
            throw std::logic_error("MappedBuffer is empty");
        if (offset > size() || bytes > size() - offset)
            throw std::out_of_range("MappedBuffer read range");
        if (bytes && !_data->coherent) {
            const auto atom = _data->atomSize;
            const vk::DeviceSize begin = offset - offset % atom;
            const vk::DeviceSize end = offset + bytes;
            const auto padding = (atom - end % atom) % atom;
            const auto length = _data->buffer->allocatedSize - end < padding ? VK_WHOLE_SIZE : end + padding - begin;
            _data->buffer->device->device.invalidateMappedMemoryRanges(
                vk::MappedMemoryRange{.memory = _data->buffer->deviceMem, .offset = begin, .size = length});
        }
        return {static_cast<const std::byte *>(_data->mapped) + offset, bytes};
    }
}
