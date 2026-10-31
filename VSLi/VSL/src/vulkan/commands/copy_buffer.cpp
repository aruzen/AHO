#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <VSL/backend/buffer_ops.hpp>
#include <VSL/sync/buffer_access.hpp>

namespace VSL_NAMESPACE::command {
    CopyBuffer::CopyBuffer(BufferAccessor* source, BufferAccessor* destination, size_t bytes,
                           size_t source_offset, size_t destination_offset, bool host_read)
        : _source(source ? source->data_.get() : nullptr), _destination(destination ? destination->data_.get() : nullptr),
          _source_owner(source ? source->data_ : nullptr), _destination_owner(destination ? destination->data_ : nullptr),
          _bytes(bytes), _source_offset(source_offset), _destination_offset(destination_offset), _host_read(host_read) {}

    CopyBuffer::CopyBuffer(BufferView source, BufferView destination, size_t bytes,
                           size_t source_offset, size_t destination_offset, bool host_read)
        : _source(source.ref().data), _destination(destination.ref().data),
          _source_owner(source.owner()), _destination_owner(destination.owner()),
          _source_use(&source.ref().use()), _destination_use(&destination.ref().use()),
          _bytes(bytes), _source_offset(source.byte_offset()), _destination_offset(destination.byte_offset()), _host_read(host_read) {
        if (source_offset > source.size() || bytes > source.size() - source_offset ||
            destination_offset > destination.size() || bytes > destination.size() - destination_offset)
            throw std::out_of_range("Buffer copy view range");
        _source_offset += source_offset;
        _destination_offset += destination_offset;
    }
    void CopyBuffer::validate() const {
        if (!_source || !_destination)
            throw std::invalid_argument("Buffer copy requires valid buffers");
        backend::validate_copy(*_source, *_destination, _bytes, _source_offset, _destination_offset, _host_read);
    }
    void CopyBuffer::invoke(CommandPool, CommandBuffer buffer, CommandManager manager) {
        validate();
        if (!buffer.data_ || !buffer.data_->command_pool || !manager.data_ ||
            buffer.data_->command_pool->device.get() != _source->device || manager.data_->device.get() != _source->device)
            throw std::invalid_argument("Buffer copy command device mismatch");
        if (_source_use) {
            const auto completion = buffer.completion();
            _source_use->check_available(completion);
            _destination_use->check_available(completion);
            buffer.retain_until_complete(_source_owner);
            buffer.retain_until_complete(_destination_owner);
            _source_use->mark_used(completion);
            _destination_use->mark_used(completion);
        } else {
            if (auto submission = buffer.data_->recording_submission)
                submission->resources.reserve(submission->resources.size() + 2);
            buffer.retain(_source_owner);
            buffer.retain(_destination_owner);
        }
        backend::record_copy(buffer.data_->command_buffers.at(buffer.get_current_buffer_idx()),
                             *_source, *_destination, _bytes, _source_offset, _destination_offset, sync::buffer_copy_visibility(_host_read));
    }
} // namespace VSL_NAMESPACE::command
