#include <AHO/resource/buffer.hpp>
#include <vector>
#include <cstring>
#include <chrono>

namespace AHO_NAMESPACE::resource {
    struct Buffer<BufferType::GENERAL>::Data {
        std::vector<std::byte> bytes;
    };
    detail::BufferMutation::BufferMutation(std::shared_ptr<BufferState> state) : _state(std::move(state)) {
        if (!_state || _state->updating)
            throw std::logic_error("Buffer is already being changed");
        _state->updating = true;
    }
    void detail::BufferMutation::finish() noexcept {
        if (_state) {
            _state->updating = false;
            _state.reset();
        }
    }
    BufferSnapshot::BufferSnapshot(VSL_NAMESPACE::BufferView storage, VSL_NAMESPACE::LogicalDeviceAccessor device,
                                   std::size_t size, VSL_NAMESPACE::ResourceUse constraints, VSL_NAMESPACE::ResourceRetention retention, bool host_readable)
        : _storage(std::move(storage)), _device(std::move(device)), _size(size), _constraints(std::move(constraints)), _retention(retention), _host_readable(host_readable) {
        if (_size > _storage.size())
            throw std::out_of_range("Buffer snapshot exceeds its storage generation");
    }
    void BufferSnapshot::read_into(std::span<std::byte> destination, std::size_t offset) const {
        const auto source = storage();
        const auto constraints = _constraints;
        const auto size = _size;
        if (offset > size || destination.size() > size - offset)
            throw std::out_of_range("Buffer snapshot read range");
        if (constraints.state)
            constraints.state->require_idle();
        source.read_into(destination, offset);
    }

    VSL_NAMESPACE::ResourceUseState* detail::BufferState::physical() const {
        if (native)
            return &native->use_state();
        return current ? &current->ref().use() : nullptr;
    }
    bool detail::BufferState::busy() const {
        const auto owner = resource.lock();
        auto* const storage = physical();
        return logical->busy() || (storage && storage->busy());
    }
    bool detail::BufferState::wait(std::uint64_t timeout) const {
        const auto owner = resource.lock();
        auto* const storage = physical();
        const auto start = std::chrono::steady_clock::now();
        if (!logical->wait(timeout))
            return false;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        const auto remaining = timeout == UINT64_MAX ? timeout : (std::uint64_t(elapsed) < timeout ? timeout - std::uint64_t(elapsed) : 0);
        return !storage || storage->wait(remaining);
    }
    void detail::BufferState::require_idle() const {
        const auto owner = resource.lock();
        auto* const storage = physical();
        logical->require_idle();
        if (storage)
            storage->require_idle();
    }
    void detail::BufferState::check_available(const VSL_NAMESPACE::Completion& completion) const {
        if (updating)
            throw std::logic_error("Buffer is being changed");
        const auto owner = resource.lock();
        auto* const storage = physical();
        logical->check_available(completion);
        if (storage)
            storage->check_available(completion);
    }
    void detail::BufferState::mark_used(VSL_NAMESPACE::Completion completion) const {
        const auto owner = resource.lock();
        auto* const storage = physical();
        check_available(completion);
        logical->mark_used(completion);
        if (storage)
            storage->mark_used(completion);
    }

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

    std::size_t BufferBinding::size() const {
        return _state ? _state->size : 0;
    }

    VSL_NAMESPACE::LogicalDeviceAccessor BufferBinding::device() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return _state->device;
    }

    std::shared_ptr<void> BufferBinding::owner() const {
        return owner_ref().share();
    }
    VSL_NAMESPACE::lifetime::OwnerRef BufferBinding::owner_ref() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return _state->resource.lock();
    }

    bool BufferBinding::host_readable() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return _state->host_readable &&
               ((_state->native && VSL_NAMESPACE::contain(_state->native->get_memory_properties(), VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE)) ||
                (_state->current && VSL_NAMESPACE::contain(_state->current->ref().properties(), VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE)));
    }

    void BufferBinding::check_available(const VSL_NAMESPACE::Completion& completion) const {
        const auto state = _state;
        if (!state)
            throw std::logic_error("Buffer binding is empty");
        state->check_available(completion);
    }

    void BufferBinding::mark_used(VSL_NAMESPACE::Completion completion) const {
        const auto state = _state;
        if (!state)
            throw std::logic_error("Buffer binding is empty");
        state->mark_used(std::move(completion));
    }
    VSL_NAMESPACE::ResourceUse BufferBinding::resource_use() const {
        if (!_state)
            throw std::logic_error("Buffer binding is empty");
        return snapshot().resource_use();
    }
    BufferSnapshot BufferBinding::snapshot() const {
        if (!_state || (!_state->native && !_state->current))
            throw std::logic_error("Buffer has no GPU storage");
        auto view = _state->native ? VSL_NAMESPACE::BufferView::FromBuffer(*_state->native) : *_state->current;
        return BufferSnapshot(std::move(view), _state->device, _state->size, {_state->logical, owner_ref()}, _state->retention, host_readable());
    }
} // namespace AHO_NAMESPACE::resource
