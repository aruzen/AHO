#include <AHO/resource/buffer.hpp>
#include <vector>
#include <cstring>

namespace AHO_NAMESPACE::resource {
    struct Buffer<BufferType::GENERAL>::Data {
        std::vector<std::byte> bytes;
    };

    Buffer<BufferType::GENERAL>::Buffer(std::size_t size) : _data(std::make_shared<Data>()) {
        _data->bytes.resize(size);
    }

    void* Buffer<BufferType::GENERAL>::data() {
        return _data ? _data->bytes.data() : nullptr;
    }

    const void* Buffer<BufferType::GENERAL>::data() const {
        return _data ? _data->bytes.data() : nullptr;
    }

    std::span<std::byte> Buffer<BufferType::GENERAL>::bytes() {
        return _data ? std::span<std::byte>(_data->bytes) : std::span<std::byte>{};
    }

    std::span<const std::byte> Buffer<BufferType::GENERAL>::bytes() const {
        return _data ? std::span<const std::byte>(_data->bytes) : std::span<const std::byte>{};
    }

    std::size_t Buffer<BufferType::GENERAL>::size() const {
        return _data ? _data->bytes.size() : 0;
    }

    std::size_t Buffer<BufferType::GENERAL>::capacity() const {
        return _data ? _data->bytes.capacity() : 0;
    }

    void Buffer<BufferType::GENERAL>::reserve(std::size_t capacity) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        _data->bytes.reserve(capacity);
    }

    void Buffer<BufferType::GENERAL>::resize(std::size_t size) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        _data->bytes.resize(size);
    }

    void Buffer<BufferType::GENERAL>::assign(std::span<const std::byte> bytes) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        if (bytes.size() > size())
            resize(bytes.size());
        if (!bytes.empty())
            std::memmove(data(), bytes.data(), bytes.size());
        resize(bytes.size());
    }

    void Buffer<BufferType::GENERAL>::write(std::span<const std::byte> bytes, std::size_t offset) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        if (offset > size() || bytes.size() > size() - offset)
            throw std::out_of_range("Buffer write range");
        if (!bytes.empty())
            std::memmove(static_cast<std::byte*>(data()) + offset, bytes.data(), bytes.size());
    }

    bool detail::BufferState::busy() const {
        const auto status = use.status();
        return status == VSL_NAMESPACE::Completion::Status::PENDING || status == VSL_NAMESPACE::Completion::Status::SUBMITTED;
    }

    void detail::BufferState::require_idle() const {
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

    bool BufferBinding::host_readable() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return _state->host_readable && _state->native &&
               VSL_NAMESPACE::contain(_state->native->get_memory_properties(), VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE);
    }

    void BufferBinding::check_available(const VSL_NAMESPACE::Completion& completion) const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        if (!completion || completion.status() != VSL_NAMESPACE::Completion::Status::PENDING)
            throw std::logic_error("Buffer transfer requires a recording phase");
        if (_state->busy() && _state->use != completion)
            throw std::logic_error("Buffer is in use by another phase");
    }

    void BufferBinding::mark_used(VSL_NAMESPACE::Completion completion) const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        _state->use = std::move(completion);
    }
} // namespace AHO_NAMESPACE::resource
