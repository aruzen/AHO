#include <AHO/resource/buffer.hpp>
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
} // namespace AHO_NAMESPACE::resource
