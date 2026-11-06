#include <AHO/resource/imported_buffer.hpp>
#include <AHO/command/buffer_upload.hpp>

namespace AHO_NAMESPACE::resource {
    ImportedBuffer::ImportedBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device) {
        if (!device.data_)
            throw std::invalid_argument("Buffer requires a device");
        _state = std::make_shared<detail::BufferState>();
        _state->device = std::move(device);
        _state->retention = VSL_NAMESPACE::ResourceRetention::COMPLETION;
    }
    ImportedBuffer::ImportedBuffer(VSL_NAMESPACE::BufferView storage) : ImportedBuffer(storage.ref().device()) {
        _state->size = storage.size();
        _state->host_readable = VSL_NAMESPACE::contain(storage.ref().properties(), VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE);
        _state->resource = storage.owner();
        _state->current = std::move(storage);
    }
    BufferBinding ImportedBuffer::binding() const {
        if (!_state)
            throw std::logic_error("Buffer is empty");
        return BufferBinding(_state);
    }
    bool ImportedBuffer::wait(std::uint64_t timeout) const {
        (void)binding();
        return _state->wait(timeout);
    }
    void ImportedBuffer::resize(size_t size) {
        (void)binding();
        _state->require_idle();
        if (size > capacity())
            throw std::out_of_range("Imported buffer capacity is fixed");
        _state->size = size;
    }
    void ImportedBuffer::write(std::span<const std::byte> bytes, size_t offset) {
        (void)binding();
        _state->require_idle();
        if (offset > size() || bytes.size() > size() - offset)
            throw std::out_of_range("Buffer write range");
        if (!bytes.empty())
            _state->current->write(bytes, offset);
    }
    void ImportedBuffer::assign(std::span<const std::byte> bytes) {
        (void)binding();
        _state->require_idle();
        if (bytes.size() > capacity())
            throw std::out_of_range("Imported buffer capacity is fixed");
        if (!bytes.empty())
            _state->current->write(bytes);
        _state->size = bytes.size();
    }
    void ImportedBuffer::read_into(std::span<std::byte> destination, size_t offset) const {
        (void)binding();
        _state->require_idle();
        if (offset > size() || destination.size() > size() - offset)
            throw std::out_of_range("Buffer read range");
        if (!destination.empty())
            _state->current->read_into(destination, offset);
    }
    AHO_NAMESPACE::command::BufferUpload ImportedBuffer::upload(const Buffer<BufferType::GENERAL>& source, size_t offset) const {
        return AHO_NAMESPACE::command::BufferUpload(binding(), source, offset);
    }
    ImportedBuffer import_buffer(VSL_NAMESPACE::BufferView storage) {
        return ImportedBuffer(std::move(storage));
    }
} // namespace AHO_NAMESPACE::resource
