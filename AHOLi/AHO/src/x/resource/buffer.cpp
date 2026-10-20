#include <AHO/x/resource/buffer.hpp>
#include "buffer_impl.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

namespace AHO_NAMESPACE::x::resource {
    struct Buffer::Data {
        std::vector<std::byte> bytes;
    };
    Buffer::Buffer(std::size_t size) : _data(std::make_shared<Data>()) { _data->bytes.resize(size); }
    std::size_t Buffer::size() const { return _data ? _data->bytes.size() : 0; }
    std::size_t Buffer::capacity() const { return _data ? _data->bytes.capacity() : 0; }
    std::span<std::byte> Buffer::data() { return _data ? std::span<std::byte>(_data->bytes) : std::span<std::byte>{}; }
    std::span<const std::byte> Buffer::data() const { return _data ? std::span<const std::byte>(_data->bytes) : std::span<const std::byte>{}; }
    void Buffer::reserve(std::size_t capacity) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        _data->bytes.reserve(capacity);
    }
    void Buffer::resize(std::size_t size) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        _data->bytes.resize(size);
    }
    void Buffer::write(std::span<const std::byte> bytes, std::size_t offset) {
        if (!_data)
            throw std::logic_error("Buffer is empty");
        if (offset > size() || bytes.size() > size() - offset)
            throw std::out_of_range("Buffer write range");
        if (!bytes.empty())
            std::memmove(_data->bytes.data() + offset, bytes.data(), bytes.size());
    }
    UploadBuffer::UploadBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t capacity)
        : _data(std::make_shared<Data>(device)) {
        if (!device._data)
            throw std::invalid_argument("UploadBuffer requires a device");
        reserve(capacity);
    }
    std::size_t UploadBuffer::size() const { return _data ? _data->size : 0; }
    std::size_t UploadBuffer::capacity() const { return _data && _data->buffer ? _data->buffer->requestedSize() : 0; }
    bool UploadBuffer::busy() const { return _data && _data->busy(); }
    bool UploadBuffer::wait(std::uint64_t timeout) const {
        if (!_data)
            throw std::logic_error("UploadBuffer is empty");
        if (!busy())
            return true;
        return _data->use.wait(timeout);
    }
    void UploadBuffer::reserve(std::size_t capacity) {
        if (!_data)
            throw std::logic_error("UploadBuffer is empty");
        _data->requireIdle();
        if (capacity <= this->capacity())
            return;
        UploadBuffer::Data::Buffer next(_data->device, capacity);
        auto mapping = next.data();
        if (_data->size)
            mapping.write(_data->mapping->read(_data->size));
        _data->buffer = std::move(next);
        _data->mapping.emplace(std::move(mapping));
    }
    void UploadBuffer::assign(std::span<const std::byte> bytes) {
        if (!_data)
            throw std::logic_error("UploadBuffer is empty");
        _data->requireIdle();
        reserve(bytes.size());
        if (!bytes.empty())
            _data->mapping->write(bytes);
        _data->size = bytes.size();
    }
    void UploadBuffer::write(std::span<const std::byte> bytes, std::size_t offset) {
        if (!_data)
            throw std::logic_error("UploadBuffer is empty");
        _data->requireIdle();
        if (offset > size() || bytes.size() > size() - offset)
            throw std::out_of_range("UploadBuffer write range");
        if (!bytes.empty())
            _data->mapping->write(bytes, offset);
    }
} // namespace AHO_NAMESPACE::x::resource
