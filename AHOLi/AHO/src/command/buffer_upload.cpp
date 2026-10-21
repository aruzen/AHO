#include <AHO/command/buffer_upload.hpp>
#include <VSL/vulkan/commands/copy_buffer.hpp>

namespace AHO_NAMESPACE::command {
    BufferUpload::BufferUpload(resource::BufferBinding destination, resource::Buffer<resource::BufferType::General> source, std::size_t offset)
        : _destination(std::move(destination)), _source(std::move(source)), _destinationOffset(offset), _sourceOffset(0) {
    }
    BufferUpload::BufferUpload(resource::BufferBinding destination, resource::BufferBinding source, std::size_t destinationOffset,
                               std::size_t sourceOffset, std::optional<std::size_t> bytes)
        : _destination(std::move(destination)), _source(std::move(source)), _destinationOffset(destinationOffset), _sourceOffset(sourceOffset), _bytes(bytes) {
    }

    void BufferUpload::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
        auto completion = buffer.completion();
        _destination.checkAvailable(completion);
        if (_destination.device() != manager.getDevice())
            throw std::invalid_argument("Buffer upload device mismatch");
        auto record = [&](resource::BufferBinding source, std::size_t sourceOffset, std::size_t bytes) {
            source.checkAvailable(completion);
            if (source.device() != _destination.device())
                throw std::invalid_argument("Buffer upload device mismatch");
            if (_destinationOffset > _destination.size() || bytes > _destination.size() - _destinationOffset)
                throw std::out_of_range("Buffer upload destination range");
            if (!bytes)
                return;
            VSL_NAMESPACE::command::CopyBuffer command(
                &source.native(), &_destination.native(), bytes,
                sourceOffset, _destinationOffset, _destination.hostReadable());
            command.validate();
            buffer.retain(source.owner());
            buffer.retain(_destination.owner());
            command.invoke(pool, buffer, manager);
            source.markUsed(completion);
            _destination.markUsed(completion);
        };
        if (auto* source = std::get_if<resource::BufferBinding>(&_source)) {
            if (_sourceOffset > source->size())
                throw std::out_of_range("Buffer upload source offset");
            const auto bytes = _bytes.value_or(source->size() - _sourceOffset);
            if (bytes > source->size() - _sourceOffset)
                throw std::out_of_range("Buffer upload source range");
            record(*source, _sourceOffset, bytes);
        } else {
            const auto& cpuSource = std::get<resource::Buffer<resource::BufferType::General>>(_source);
            if (_destinationOffset > _destination.size() || cpuSource.size() > _destination.size() - _destinationOffset)
                throw std::out_of_range("Buffer upload destination range");
            if (!cpuSource.size())
                return;
            if (!_destination._state->staging) {
                resource::Buffer<resource::BufferType::GPUFriendly> staging(_destination.device());
                _destination._state->staging = staging.binding()._state;
            }

            resource::Buffer<resource::BufferType::GPUFriendly> staging(_destination._state->staging);
            staging.assign(cpuSource.bytes());
            record(staging.binding(), 0, cpuSource.size());
        }
    }
} // namespace AHO_NAMESPACE::command
