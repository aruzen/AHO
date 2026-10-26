#include <AHO/command/buffer_upload.hpp>
#include <VSL/vulkan/commands/copy_buffer.hpp>

namespace AHO_NAMESPACE::command {
    BufferUpload::BufferUpload(resource::BufferBinding destination, resource::Buffer<resource::BufferType::GENERAL> source, std::size_t offset)
        : _destination(std::move(destination)), _source(std::move(source)), _destination_offset(offset), _source_offset(0) {
    }
    BufferUpload::BufferUpload(resource::BufferBinding destination, resource::BufferBinding source, std::size_t destination_offset,
                               std::size_t source_offset, std::optional<std::size_t> bytes)
        : _destination(std::move(destination)), _source(std::move(source)), _destination_offset(destination_offset), _source_offset(source_offset), _bytes(bytes) {
    }

    void BufferUpload::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
        auto completion = buffer.completion();
        _destination.check_available(completion);
        if (_destination.device() != manager.get_device())
            throw std::invalid_argument("Buffer upload device mismatch");
        auto record = [&](resource::BufferBinding source, std::size_t source_offset, std::size_t bytes) {
            source.check_available(completion);
            if (source.device() != _destination.device())
                throw std::invalid_argument("Buffer upload device mismatch");
            if (_destination_offset > _destination.size() || bytes > _destination.size() - _destination_offset)
                throw std::out_of_range("Buffer upload destination range");
            if (!bytes)
                return;
            VSL_NAMESPACE::command::CopyBuffer command(
                &source.native(), &_destination.native(), bytes,
                source_offset, _destination_offset, _destination.host_readable());
            command.validate();
            buffer.retain(source.owner());
            buffer.retain(_destination.owner());
            command.invoke(pool, buffer, manager);
            source.mark_used(completion);
            _destination.mark_used(completion);
        };
        if (auto* source = std::get_if<resource::BufferBinding>(&_source)) {
            if (_source_offset > source->size())
                throw std::out_of_range("Buffer upload source offset");
            const auto bytes = _bytes.value_or(source->size() - _source_offset);
            if (bytes > source->size() - _source_offset)
                throw std::out_of_range("Buffer upload source range");
            record(*source, _source_offset, bytes);
        } else {
            const auto& cpu_source = std::get<resource::Buffer<resource::BufferType::GENERAL>>(_source);
            if (_destination_offset > _destination.size() || cpu_source.size() > _destination.size() - _destination_offset)
                throw std::out_of_range("Buffer upload destination range");
            if (!cpu_source.size())
                return;
            if (!_destination._state->staging) {
                _destination._state->staging = std::make_shared<resource::Buffer<resource::BufferType::GPU_FRIENDLY>>(_destination.device());
            }

            auto& staging = *_destination._state->staging;
            staging.assign(cpu_source.bytes());
            record(staging.binding(), 0, cpu_source.size());
        }
    }
} // namespace AHO_NAMESPACE::command
