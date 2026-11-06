#include <AHO/resource/arena_buffer.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include <vector>

namespace AHO_NAMESPACE::resource::detail {
    struct ArenaBufferState : BufferState {
        ArenaBufferFactory factory;
        ArenaBufferState(VSL_NAMESPACE::LogicalDeviceAccessor device, ArenaBufferFactory factory)
            : factory(std::move(factory)) { this->device = std::move(device); }
    };
} // namespace AHO_NAMESPACE::resource::detail

namespace AHO_NAMESPACE::resource {
    ArenaBuffer::ArenaBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, ArenaBufferFactory factory)
        : Base(std::make_shared<detail::ArenaBufferState>(std::move(device), std::move(factory))) {
        if (size)
            publish(allocate(size));
        state().host_readable = true;
        state().size = size;
    }
    VSL_NAMESPACE::BufferView ArenaBuffer::allocate(size_t bytes) const {
        const auto& state = static_cast<const detail::ArenaBufferState&>(this->state());
        auto view = [&] {
            if (state.factory)
                return state.factory(state.device, bytes);
            const VSL_NAMESPACE::BufferCreateInfo info{bytes, detail::STANDARD_BUFFER_USAGE};
            return VSL_NAMESPACE::lifetime::BufferArena(state.device, std::span(&info, 1)).view(0);
        }();
        if (view.ref().device() != state.device || view.size() < bytes ||
            !VSL_NAMESPACE::contain(view.ref().properties(), VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE) ||
            !VSL_NAMESPACE::contain(view.ref().usage(), detail::STANDARD_BUFFER_USAGE) ||
            (state.current && view.ref().data == state.current->ref().data))
            throw std::invalid_argument("Arena buffer factory must return a new, sufficient host-visible generation on the same device");
        return view;
    }
    void ArenaBuffer::publish(VSL_NAMESPACE::BufferView storage) {
        auto& state = this->state();
        state.compatibility.reset();
        state.resource = storage.owner();
        state.current = std::move(storage);
    }
    void ArenaBuffer::replace(size_t bytes, bool preserve) {
        auto next = allocate(bytes);
        if (preserve && size()) {
            std::vector<std::byte> content(size());
            state().current->read_into(content);
            next.write(content);
        }
        publish(std::move(next));
    }
    void ArenaBuffer::reserve(size_t capacity) {
        state().require_idle();
        if (capacity > this->capacity())
            replace(capacity, true);
    }
    void ArenaBuffer::resize(size_t size) {
        reserve(size);
        state().size = size;
    }
    void ArenaBuffer::assign(std::span<const std::byte> bytes) {
        state().require_idle();
        if (bytes.size() > capacity()) {
            auto next = allocate(bytes.size());
            next.write(bytes);
            publish(std::move(next));
            state().size = bytes.size();
        } else
            Base::assign(bytes);
    }
    ArenaBuffer make_arena_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, ArenaBufferFactory factory) {
        return ArenaBuffer(std::move(device), size, std::move(factory));
    }
} // namespace AHO_NAMESPACE::resource
