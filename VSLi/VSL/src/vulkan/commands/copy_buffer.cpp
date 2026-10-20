#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::command {
    CopyBuffer::CopyBuffer(BufferAccessor* source, BufferAccessor* destination, std::size_t bytes,
                           std::size_t sourceOffset, std::size_t destinationOffset, bool hostRead)
        : source_(source ? source->_data : nullptr), destination_(destination ? destination->_data : nullptr),
          bytes_(bytes), sourceOffset_(sourceOffset), destinationOffset_(destinationOffset), hostRead_(hostRead) {}
    void CopyBuffer::validate() const {
        if (!source_ || !destination_ || !source_->buffer || !destination_->buffer || source_->device != destination_->device)
            throw std::invalid_argument("Buffer copy requires buffers on the same device");
        if (!contain(source_->usage, MemoryType::TransferSource) || !contain(destination_->usage, MemoryType::TransferDestination))
            throw std::invalid_argument("Buffer copy requires transfer usage");
        if (!bytes_ || sourceOffset_ > source_->requirementSize || bytes_ > source_->requirementSize - sourceOffset_ ||
            destinationOffset_ > destination_->requirementSize || bytes_ > destination_->requirementSize - destinationOffset_)
            throw std::out_of_range("Buffer copy range");
        if (source_->deviceMem == destination_->deviceMem && sourceOffset_ < destinationOffset_ + bytes_ && destinationOffset_ < sourceOffset_ + bytes_)
            throw std::invalid_argument("Buffer copy ranges must not overlap");
        if (hostRead_ && !contain(destination_->properties, MemoryProperty::HostVisible))
            throw std::invalid_argument("Host read requires host-visible buffer memory");
    }
    void CopyBuffer::invoke(CommandPool, CommandBuffer buffer, CommandManager manager) {
        validate();
        if (!buffer._data || !buffer._data->commandPool || !manager._data ||
            buffer._data->commandPool->device != source_->device || manager.getDevice()._data != source_->device)
            throw std::invalid_argument("Buffer copy command device mismatch");
        if (auto submission = buffer._data->recordingSubmission)
            submission->resources.reserve(submission->resources.size() + 2);
        buffer.retain(source_);
        buffer.retain(destination_);
        auto command = buffer._data->commandBuffers.at(buffer.getCurrentBufferIdx());
        const vk::BufferMemoryBarrier before[] = {
            {.srcAccessMask = vk::AccessFlagBits::eMemoryWrite, .dstAccessMask = vk::AccessFlagBits::eTransferRead, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .buffer = source_->buffer, .offset = sourceOffset_, .size = bytes_},
            {.srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite, .dstAccessMask = vk::AccessFlagBits::eTransferWrite, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .buffer = destination_->buffer, .offset = destinationOffset_, .size = bytes_}};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, before, {});
        command.copyBuffer(source_->buffer, destination_->buffer, vk::BufferCopy{sourceOffset_, destinationOffset_, bytes_});
        vk::PipelineStageFlags stages = vk::PipelineStageFlagBits::eAllCommands;
        vk::AccessFlags access = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        if (hostRead_) {
            stages |= vk::PipelineStageFlagBits::eHost;
            access |= vk::AccessFlagBits::eHostRead;
        }
        const vk::BufferMemoryBarrier after{.srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = access, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .buffer = destination_->buffer, .offset = destinationOffset_, .size = bytes_};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, stages, {}, {}, after, {});
    }
} // namespace VSL_NAMESPACE::command
