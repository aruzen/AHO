#include <AHO/resource/arena_buffer.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include <vector>

namespace AHO_NAMESPACE::resource::detail {
    struct ArenaBufferProvider {
        ArenaBufferFactory factory;
    };
    struct ArenaBufferGeneration {
        std::shared_ptr<ArenaBufferProvider> provider;
        VSL_NAMESPACE::BufferView storage;
    };
} // namespace AHO_NAMESPACE::resource::detail

namespace AHO_NAMESPACE::resource {
    ArenaBuffer::ArenaBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, ArenaBufferFactory factory)
        : ImportedBuffer(device), _provider(std::make_shared<detail::ArenaBufferProvider>(std::move(factory))) {
        if (size)
            publish(allocate(size));
        _state->host_readable = true;
        _state->size = size;
    }
    VSL_NAMESPACE::BufferView ArenaBuffer::allocate(size_t bytes) const {
        auto view = [&] {
            if (_provider->factory)
                return _provider->factory(_state->device, bytes);
            const VSL_NAMESPACE::BufferCreateInfo info{bytes, detail::STANDARD_BUFFER_USAGE};
            return VSL_NAMESPACE::lifetime::BufferArena(_state->device, std::span(&info, 1)).view(0);
        }();
        if (view.ref().device() != _state->device || view.size() < bytes ||
            !VSL_NAMESPACE::contain(view.ref().properties(), VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE) ||
            !VSL_NAMESPACE::contain(view.ref().usage(), detail::STANDARD_BUFFER_USAGE) ||
            (_state->current && view.ref().data == _state->current->ref().data))
            throw std::invalid_argument("Arena buffer factory must return a new, sufficient host-visible generation on the same device");
        return view;
    }
    void ArenaBuffer::publish(VSL_NAMESPACE::BufferView storage) {
        auto generation = std::make_shared<detail::ArenaBufferGeneration>(_provider, std::move(storage));
        const auto& view = generation->storage;
        VSL_NAMESPACE::BufferView next(view.ref(), generation, view.byte_offset(), view.size());
        _state->compatibility.reset();
        _state->current = std::move(next);
        _state->resource = generation;
    }
    void ArenaBuffer::replace(size_t bytes, bool preserve) {
        auto next = allocate(bytes);
        if (preserve && size()) {
            std::vector<std::byte> content(size());
            _state->current->read_into(content);
            next.write(content);
        }
        publish(std::move(next));
    }
    void ArenaBuffer::reserve(size_t capacity) {
        (void)binding();
        _state->require_idle();
        if (capacity > this->capacity())
            replace(capacity, true);
    }
    void ArenaBuffer::resize(size_t size) {
        reserve(size);
        _state->size = size;
    }
    void ArenaBuffer::assign(std::span<const std::byte> bytes) {
        (void)binding();
        _state->require_idle();
        if (bytes.size() > capacity()) {
            auto next = allocate(bytes.size());
            next.write(bytes);
            publish(std::move(next));
            _state->size = bytes.size();
        } else
            ImportedBuffer::assign(bytes);
    }
    ArenaBuffer make_arena_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, ArenaBufferFactory factory) {
        return ArenaBuffer(std::move(device), size, std::move(factory));
    }
} // namespace AHO_NAMESPACE::resource
