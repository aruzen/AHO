#include <VSL/backend/buffer_ops.hpp>
#include <VSL/exceptions.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::backend {
    void initialize_buffer(impl::BufferImpl& target, impl::LogicalDeviceImpl& device, const BufferCreateInfo& parameters) {
        const auto size = parameters.bytes;
        const auto mem_type = parameters.usage;
        const auto mem_property = parameters.properties;
        const auto sharing_mode = parameters.sharing;
        try {
            if (!device.device || !size || mem_type == MemoryType::NONE)
                throw std::invalid_argument("Buffer requires a device, positive size and usage");
            auto* data = &target;
            data->device = &device;
            data->native_device = device.device;
            data->usage = mem_type;
            data->requirement_size = size;
            vk::BufferCreateInfo info{.size = size, .usage = vk::BufferUsageFlags{static_cast<unsigned int>(mem_type)}, .sharingMode = static_cast<vk::SharingMode>(sharing_mode)};
            const std::uint32_t families[] = {device.graphics_family, device.present_family};
            if (sharing_mode == SharingMode::CONCURRENT) {
                if (families[0] == families[1])
                    info.sharingMode = vk::SharingMode::eExclusive;
                else {
                    info.queueFamilyIndexCount = 2;
                    info.pQueueFamilyIndices = families;
                }
            }
            data->buffer = data->native_device.createBuffer(info);
            const auto requirements = data->native_device.getBufferMemoryRequirements(data->buffer);
            const auto properties = device.parent_device->device.getMemoryProperties();
            const auto required = vk::MemoryPropertyFlags{static_cast<unsigned int>(mem_property)};
            std::optional<std::uint32_t> selected;
            for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
                const auto flags = properties.memoryTypes[i].propertyFlags;
                if (!(requirements.memoryTypeBits & (1u << i)) || (flags & required) != required)
                    continue;
                if (!selected)
                    selected = i;
                if (contain(mem_property, MemoryProperty::HOST_VISIBLE) && (flags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
                    selected = i;
                    break;
                }
            }
            if (!selected)
                throw exceptions::RuntimeException("MemoryTypeNotFound", "No memory type satisfies the buffer requirements");
            data->properties = static_cast<MemoryProperty>(static_cast<unsigned int>(properties.memoryTypes[*selected].propertyFlags));
            data->allocated_size = requirements.size;
            data->atom_size = device.parent_device->device.getProperties().limits.nonCoherentAtomSize;
            data->device_mem = data->native_device.allocateMemory({.allocationSize = requirements.size, .memoryTypeIndex = *selected});
            data->native_device.bindBufferMemory(data->buffer, data->device_mem, 0);
        } catch (...) {
            destroy_buffer(target);
            throw;
        }
    }
    void destroy_buffer(impl::BufferImpl& target) noexcept {
        if (!target.device)
            return;
        if (target.mapped)
            target.native_device.unmapMemory(target.device_mem);
        target.native_device.destroyBuffer(target.buffer);
        target.native_device.freeMemory(target.device_mem);
        target.mapped = nullptr;
        target.buffer = nullptr;
        target.device_mem = nullptr;
    }
    void* map_buffer(impl::BufferImpl& target, size_t offset, size_t bytes) {
        if (!contain(target.properties, MemoryProperty::HOST_VISIBLE))
            throw std::logic_error("Buffer is not host-visible");
        if (offset > target.requirement_size || bytes > target.requirement_size - offset)
            throw std::out_of_range("Buffer mapping range");
        if (!target.mapped)
            target.mapped = target.native_device.mapMemory(target.device_mem, 0, VK_WHOLE_SIZE);
        return static_cast<std::byte*>(target.mapped) + offset;
    }
    void synchronize_buffer(impl::BufferImpl& target, size_t offset, size_t bytes, bool flush) {
        if (!target.mapped || offset > target.requirement_size || bytes > target.requirement_size - offset)
            throw std::out_of_range("Buffer synchronization range");
        if (!bytes || contain(target.properties, MemoryProperty::HOST_COHERENT))
            return;
        const auto atom = target.atom_size;
        const vk::DeviceSize begin = offset - offset % atom;
        const vk::DeviceSize end = offset + bytes;
        const auto padding = (atom - end % atom) % atom;
        const auto length = target.allocated_size - end < padding ? VK_WHOLE_SIZE : end + padding - begin;
        const vk::MappedMemoryRange range{.memory = target.device_mem, .offset = begin, .size = length};
        if (flush)
            target.native_device.flushMappedMemoryRanges(range);
        else
            target.native_device.invalidateMappedMemoryRanges(range);
    }
    void validate_copy(const impl::BufferImpl& source, const impl::BufferImpl& destination,
                       size_t bytes, size_t source_offset, size_t destination_offset, bool host_read) {
        if (!source.buffer || !destination.buffer ||
            source.device != destination.device)
            throw std::invalid_argument("Buffer copy requires buffers on the same device");
        if (!contain(source.usage, MemoryType::TRANSFER_SOURCE) || !contain(destination.usage, MemoryType::TRANSFER_DESTINATION))
            throw std::invalid_argument("Buffer copy requires transfer usage");
        if (!bytes || source_offset > source.requirement_size || bytes > source.requirement_size - source_offset ||
            destination_offset > destination.requirement_size || bytes > destination.requirement_size - destination_offset)
            throw std::out_of_range("Buffer copy range");
        if (source.device_mem == destination.device_mem &&
            source_offset < destination_offset + bytes && destination_offset < source_offset + bytes)
            throw std::invalid_argument("Buffer copy ranges must not overlap");
        if (host_read && !contain(destination.properties, MemoryProperty::HOST_VISIBLE))
            throw std::invalid_argument("HOST read requires host-visible buffer memory");
    }
    void record_copy(vk::CommandBuffer command, const impl::BufferImpl& source, const impl::BufferImpl& destination,
                     size_t bytes, size_t source_offset, size_t destination_offset, const BufferCopyVisibility& visibility) {
        const vk::BufferMemoryBarrier before[] = {
            {
                .srcAccessMask = visibility.source_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = source.buffer,
                .offset = source_offset,
                .size = bytes,
            },
            {
                .srcAccessMask = visibility.destination_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = destination.buffer,
                .offset = destination_offset,
                .size = bytes,
            },
        };
        command.pipelineBarrier(visibility.before_source, visibility.before_destination, {}, {}, before, {});
        command.copyBuffer(source.buffer, destination.buffer, vk::BufferCopy{source_offset, destination_offset, bytes});

        const vk::BufferMemoryBarrier after{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = visibility.after_access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = destination.buffer,
            .offset = destination_offset,
            .size = bytes,
        };

        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, visibility.after_destination, {}, {}, after, {});
    }
} // namespace VSL_NAMESPACE::backend
