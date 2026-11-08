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
            Publish(state(), Prepare(state_owner(), size, false));
        state().host_readable = true;
        state().size = size;
    }
    VSL_NAMESPACE::BufferView ArenaBuffer::Prepare(const std::shared_ptr<detail::BufferState>& owner, size_t bytes, bool preserve) {
        const auto& state = static_cast<const detail::ArenaBufferState&>(*owner);
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
        if (preserve && state.size) {
            std::vector<std::byte> content(state.size);
            state.current->read_into(content);
            view.write(content);
        }
        return view;
    }
    auto ArenaBuffer::Publish(detail::BufferState& state, VSL_NAMESPACE::BufferView storage) -> RetiredStorage {
        auto owner = storage.owner_ref().weak();
        state.require_idle();
        RetiredStorage previous{std::move(state.current), std::move(state.compatibility)};
        state.resource = std::move(owner);
        state.current = std::move(storage);
        return previous;
    }
    void ArenaBuffer::reserve(size_t capacity) {
        const auto state = state_owner();
        detail::BufferMutation mutation(state);
        state->require_idle();
        RetiredStorage previous;
        if (capacity > (state->current ? state->current->size() : 0))
            previous = Publish(*state, Prepare(state, capacity, true));
        mutation.finish();
    }
    void ArenaBuffer::resize(size_t size) {
        const auto state = state_owner();
        detail::BufferMutation mutation(state);
        state->require_idle();
        RetiredStorage previous;
        if (size > (state->current ? state->current->size() : 0))
            previous = Publish(*state, Prepare(state, size, true));
        state->size = size;
        mutation.finish();
    }
    void ArenaBuffer::assign(std::span<const std::byte> bytes) {
        const auto state = state_owner();
        detail::BufferMutation mutation(state);
        state->require_idle();
        RetiredStorage previous;
        if (bytes.size() > (state->current ? state->current->size() : 0)) {
            auto next = Prepare(state, bytes.size(), false);
            next.write(bytes);
            previous = Publish(*state, std::move(next));
        } else if (!bytes.empty())
            state->current->write(bytes);
        state->size = bytes.size();
        mutation.finish();
    }
    ArenaBuffer make_arena_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, ArenaBufferFactory factory) {
        return ArenaBuffer(std::move(device), size, std::move(factory));
    }
} // namespace AHO_NAMESPACE::resource
