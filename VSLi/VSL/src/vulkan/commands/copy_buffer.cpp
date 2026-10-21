#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::command {
    CopyBuffer::CopyBuffer(BufferAccessor* source, BufferAccessor* destination, std::size_t bytes,
                           std::size_t sourceOffset, std::size_t destinationOffset, bool hostRead)
        : _source(source ? source->_data : nullptr),
          _destination(destination ? destination->_data : nullptr),
          _bytes(bytes), _sourceOffset(sourceOffset), _destinationOffset(destinationOffset), _hostRead(hostRead) {
    }

    void CopyBuffer::validate() const {
        if (!_source || !_destination || !_source->buffer || !_destination->buffer ||
            _source->device != _destination->device)
            throw std::invalid_argument("Buffer copy requires buffers on the same device");
        if (!contain(_source->usage, MemoryType::TransferSource) || !contain(_destination->usage, MemoryType::TransferDestination))
            throw std::invalid_argument("Buffer copy requires transfer usage");
        if (!_bytes || _sourceOffset > _source->requirementSize || _bytes > _source->requirementSize - _sourceOffset ||
            _destinationOffset > _destination->requirementSize || _bytes > _destination->requirementSize - _destinationOffset)
            throw std::out_of_range("Buffer copy range");
        if (_source->deviceMem == _destination->deviceMem &&
            _sourceOffset < _destinationOffset + _bytes && _destinationOffset < _sourceOffset + _bytes)
            throw std::invalid_argument("Buffer copy ranges must not overlap");
        if (_hostRead && !contain(_destination->properties, MemoryProperty::HostVisible))
            throw std::invalid_argument("Host read requires host-visible buffer memory");
    }

    void CopyBuffer::invoke(CommandPool, CommandBuffer buffer, CommandManager manager) {
        validate();
        if (!buffer._data || !buffer._data->commandPool || !manager._data ||
            buffer._data->commandPool->device != _source->device || manager.getDevice()._data != _source->device)
            throw std::invalid_argument("Buffer copy command device mismatch");
        if (auto submission = buffer._data->recordingSubmission)
            submission->resources.reserve(submission->resources.size() + 2);

        buffer.retain(_source);
        buffer.retain(_destination);

        auto command = buffer._data->commandBuffers.at(buffer.getCurrentBufferIdx());
        const vk::BufferMemoryBarrier before[] = {
            {
                .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = _source->buffer,
                .offset = _sourceOffset,
                .size = _bytes,
            },
            {
                .srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = _destination->buffer,
                .offset = _destinationOffset,
                .size = _bytes,
            },
        };
        command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, before, {});
        command.copyBuffer(_source->buffer, _destination->buffer, vk::BufferCopy{_sourceOffset, _destinationOffset, _bytes});

        vk::PipelineStageFlags stages = vk::PipelineStageFlagBits::eAllCommands;
        vk::AccessFlags access = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        if (_hostRead) {
            stages |= vk::PipelineStageFlagBits::eHost;
            access |= vk::AccessFlagBits::eHostRead;
        }
        const vk::BufferMemoryBarrier after{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = _destination->buffer,
            .offset = _destinationOffset,
            .size = _bytes,
        };

        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, stages, {}, {}, after, {});
    }
} // namespace VSL_NAMESPACE::command
