#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::command {
    CopyBuffer::CopyBuffer(BufferAccessor* source, BufferAccessor* destination, std::size_t bytes,
                           std::size_t source_offset, std::size_t destination_offset, bool host_read)
        : _source(source ? source->data_ : nullptr),
          _destination(destination ? destination->data_ : nullptr),
          _bytes(bytes), _source_offset(source_offset), _destination_offset(destination_offset), _host_read(host_read) {
    }

    void CopyBuffer::validate() const {
        if (!_source || !_destination || !_source->buffer || !_destination->buffer ||
            _source->device != _destination->device)
            throw std::invalid_argument("Buffer copy requires buffers on the same device");
        if (!contain(_source->usage, MemoryType::TRANSFER_SOURCE) || !contain(_destination->usage, MemoryType::TRANSFER_DESTINATION))
            throw std::invalid_argument("Buffer copy requires transfer usage");
        if (!_bytes || _source_offset > _source->requirement_size || _bytes > _source->requirement_size - _source_offset ||
            _destination_offset > _destination->requirement_size || _bytes > _destination->requirement_size - _destination_offset)
            throw std::out_of_range("Buffer copy range");
        if (_source->device_mem == _destination->device_mem &&
            _source_offset < _destination_offset + _bytes && _destination_offset < _source_offset + _bytes)
            throw std::invalid_argument("Buffer copy ranges must not overlap");
        if (_host_read && !contain(_destination->properties, MemoryProperty::HOST_VISIBLE))
            throw std::invalid_argument("HOST read requires host-visible buffer memory");
    }

    void CopyBuffer::invoke(CommandPool, CommandBuffer buffer, CommandManager manager) {
        validate();
        if (!buffer.data_ || !buffer.data_->command_pool || !manager.data_ ||
            buffer.data_->command_pool->device != _source->device || manager.get_device().data_ != _source->device)
            throw std::invalid_argument("Buffer copy command device mismatch");
        if (auto submission = buffer.data_->recording_submission)
            submission->resources.reserve(submission->resources.size() + 2);

        buffer.retain(_source);
        buffer.retain(_destination);

        auto command = buffer.data_->command_buffers.at(buffer.get_current_buffer_idx());
        const vk::BufferMemoryBarrier before[] = {
            {
                .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = _source->buffer,
                .offset = _source_offset,
                .size = _bytes,
            },
            {
                .srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = _destination->buffer,
                .offset = _destination_offset,
                .size = _bytes,
            },
        };
        command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, before, {});
        command.copyBuffer(_source->buffer, _destination->buffer, vk::BufferCopy{_source_offset, _destination_offset, _bytes});

        vk::PipelineStageFlags stages = vk::PipelineStageFlagBits::eAllCommands;
        vk::AccessFlags access = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        if (_host_read) {
            stages |= vk::PipelineStageFlagBits::eHost;
            access |= vk::AccessFlagBits::eHostRead;
        }
        const vk::BufferMemoryBarrier after{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = _destination->buffer,
            .offset = _destination_offset,
            .size = _bytes,
        };

        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, stages, {}, {}, after, {});
    }
} // namespace VSL_NAMESPACE::command
