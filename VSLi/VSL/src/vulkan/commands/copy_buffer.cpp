#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <VSL/operation/buffer_ops.hpp>
#include <VSL/sync/buffer_access.hpp>
#include <VSL/lifetime/buffer_access.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE::command {
    CopyBuffer::CopyBuffer(BufferAccessor* source, BufferAccessor* destination, size_t bytes,
                           size_t source_offset, size_t destination_offset, bool host_read)
        : _source(source ? BufferAccess::Owned(*(source)).get() : nullptr), _destination(destination ? BufferAccess::Owned(*(destination)).get() : nullptr),
          _source_owner(source ? BufferAccess::Owned(*(source)) : nullptr), _destination_owner(destination ? BufferAccess::Owned(*(destination)) : nullptr),
          _bytes(bytes), _source_offset(source_offset), _destination_offset(destination_offset), _host_read(host_read) {}

    CopyBuffer::CopyBuffer(BufferView source, BufferView destination, size_t bytes,
                           size_t source_offset, size_t destination_offset, bool host_read)
        : _source(lifetime::detail::BufferAccess::Impl(source.ref())), _destination(lifetime::detail::BufferAccess::Impl(destination.ref())),
          _source_owner(source.owner_ref()), _destination_owner(destination.owner_ref()),
          _release_on_completion(true),
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
        operation::validate_copy(*_source, *_destination, _bytes, _source_offset, _destination_offset, _host_read);
    }
    void CopyBuffer::invoke(CommandPool, CommandBuffer buffer, CommandManager manager) {
        const auto copy = *this;
        copy.validate();
        if (!CommandAccess::Data(buffer) || !CommandAccess::Data(buffer)->command_pool || !CommandAccess::Data(manager) ||
            CommandAccess::Data(buffer)->command_pool->device != copy._source->device || CommandAccess::Data(manager)->device.get() != copy._source->device)
            throw std::invalid_argument("Buffer copy command device mismatch");
        const NativeRecordingContext recording(buffer);
        const auto completion = recording.completion();
        copy._source->use->check_available(completion);
        recording.require_current();
        copy._destination->use->check_available(completion);
        recording.require_current();
        const auto retention = copy._release_on_completion ? ResourceRetention::COMPLETION : ResourceRetention::SUBMISSION;
        recording.retain(copy._source_owner, retention);
        recording.retain(copy._destination_owner, retention);
        copy._source->use->mark_used(completion);
        recording.require_current();
        copy._destination->use->mark_used(completion);
        recording.require_current();
        operation::record_copy(vk::CommandBuffer(recording.native_command_buffer()),
                               *copy._source, *copy._destination, copy._bytes, copy._source_offset, copy._destination_offset, sync::buffer_copy_visibility(copy._host_read));
    }
} // namespace VSL_NAMESPACE::command
