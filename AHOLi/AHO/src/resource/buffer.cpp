#include <AHO/resource/buffer.hpp>
#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <vector>
#include <cstring>

namespace AHO_NAMESPACE::resource {
    struct Buffer<BufferType::General>::Data {
        std::vector<std::byte> bytes;
    };

    Buffer<BufferType::General>::Buffer(std::size_t size) : _data(std::make_shared<Data>()) {
        _data->bytes.resize(size);
    }

    void* Buffer<BufferType::General>::data() {
        return _data ? _data->bytes.data() : nullptr;
    }

    const void* Buffer<BufferType::General>::data() const {
        return _data ? _data->bytes.data() : nullptr;
    }

    std::span<std::byte> Buffer<BufferType::General>::bytes() {
        return _data ? std::span<std::byte>(_data->bytes) : std::span<std::byte>{};
    }

    std::span<const std::byte> Buffer<BufferType::General>::bytes() const {
        return _data ? std::span<const std::byte>(_data->bytes) : std::span<const std::byte>{};
    }

    std::size_t Buffer<BufferType::General>::size() const {
        return _data ? _data->bytes.size() : 0;
    }

    std::size_t Buffer<BufferType::General>::capacity() const {
        return _data ? _data->bytes.capacity() : 0;
    }

    void Buffer<BufferType::General>::reserve(std::size_t capacity) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        _data->bytes.reserve(capacity);
    }

    void Buffer<BufferType::General>::resize(std::size_t size) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        _data->bytes.resize(size);
    }

    void Buffer<BufferType::General>::assign(std::span<const std::byte> bytes) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        if (bytes.size() > size())
            resize(bytes.size());
        if (!bytes.empty())
            std::memmove(data(), bytes.data(), bytes.size());
        resize(bytes.size());
    }

    void Buffer<BufferType::General>::write(std::span<const std::byte> bytes, std::size_t offset) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        if (offset > size() || bytes.size() > size() - offset)
            throw std::out_of_range("Buffer write range");
        if (!bytes.empty())
            std::memmove(static_cast<std::byte*>(data()) + offset, bytes.data(), bytes.size());
    }

    bool detail::BufferState::busy() const {
        const auto status = use.status();
        return status == VSL_NAMESPACE::Completion::Status::Pending || status == VSL_NAMESPACE::Completion::Status::Submitted;
    }

    void detail::BufferState::requireIdle() const {
        if (busy())
            throw std::logic_error("Buffer is still in use");
    }

    std::size_t BufferBinding::size() const {
        return _state ? _state->size : 0;
    }

    VSL_NAMESPACE::LogicalDeviceAccessor BufferBinding::device() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return _state->device;
    }

    VSL_NAMESPACE::BufferAccessor& BufferBinding::native() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        if (!_state->native)
            throw std::logic_error("Buffer has no GPU storage");
        return *_state->native;
    }

    std::shared_ptr<void> BufferBinding::owner() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return _state->resource;
    }

    bool BufferBinding::hostReadable() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return _state->hostReadable && _state->native &&
               VSL_NAMESPACE::contain(_state->native->getMemoryProperties(), VSL_NAMESPACE::MemoryProperty::HostVisible);
    }

    void BufferBinding::checkAvailable(const VSL_NAMESPACE::Completion& completion) const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        if (!completion || completion.status() != VSL_NAMESPACE::Completion::Status::Pending)
            throw std::logic_error("Buffer transfer requires a recording phase");
        if (_state->busy() && _state->use != completion)
            throw std::logic_error("Buffer is in use by another phase");
    }

    void BufferBinding::markUsed(VSL_NAMESPACE::Completion completion) const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        _state->use = std::move(completion);
    }

    BufferUpload::BufferUpload(BufferBinding destination, Buffer<BufferType::General> source, std::size_t offset)
        : _destination(std::move(destination)), _source(std::move(source)), _destinationOffset(offset), _sourceOffset(0) {
    }
    BufferUpload::BufferUpload(BufferBinding destination, BufferBinding source, std::size_t destinationOffset,
                               std::size_t sourceOffset, std::optional<std::size_t> bytes)
        : _destination(std::move(destination)), _source(std::move(source)), _destinationOffset(destinationOffset), _sourceOffset(sourceOffset), _bytes(bytes) {
    }

    void BufferUpload::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
        auto completion = buffer.completion();
        _destination.checkAvailable(completion);
        if (_destination.device() != manager.getDevice())
            throw std::invalid_argument("Buffer upload device mismatch");
        auto record = [&](BufferBinding source, std::size_t sourceOffset, std::size_t bytes) {
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
        if (auto* source = std::get_if<BufferBinding>(&_source)) {
            if (_sourceOffset > source->size())
                throw std::out_of_range("Buffer upload source offset");
            const auto bytes = _bytes.value_or(source->size() - _sourceOffset);
            if (bytes > source->size() - _sourceOffset)
                throw std::out_of_range("Buffer upload source range");
            record(*source, _sourceOffset, bytes);
        } else {
            const auto& cpuSource = std::get<Buffer<BufferType::General>>(_source);
            if (_destinationOffset > _destination.size() || cpuSource.size() > _destination.size() - _destinationOffset)
                throw std::out_of_range("Buffer upload destination range");
            if (!cpuSource.size())
                return;
            if (!_destination._state->staging) {
                Buffer<BufferType::GPUFriendly> staging(_destination.device());
                _destination._state->staging = staging.binding()._state;
            }

            Buffer<BufferType::GPUFriendly> staging(_destination._state->staging);
            staging.assign(cpuSource.bytes());
            record(staging.binding(), 0, cpuSource.size());
        }
    }
} // namespace AHO_NAMESPACE::resource
