#include <AHO/resource/imported_buffer.hpp>
#include <AHO/command/buffer_upload.hpp>

namespace AHO_NAMESPACE::resource::detail {
    ViewBufferBase::ViewBufferBase(std::shared_ptr<BufferState> state) : _state(std::move(state)) {
        if (!_state || !_state->device.data_)
            throw std::invalid_argument("Buffer requires a device");
        _state->retention = VSL_NAMESPACE::ResourceRetention::COMPLETION;
    }
    ViewBufferBase::ViewBufferBase(VSL_NAMESPACE::BufferView storage) : _state(std::make_shared<BufferState>()) {
        _state->device = storage.ref().device();
        _state->retention = VSL_NAMESPACE::ResourceRetention::COMPLETION;
        _state->size = storage.size();
        _state->host_readable = VSL_NAMESPACE::contain(storage.ref().properties(), VSL_NAMESPACE::MemoryProperty::HOST_VISIBLE);
        _state->resource = storage.owner_ref().weak();
        _state->current = std::move(storage);
    }
    BufferState& ViewBufferBase::state() const {
        if (!_state)
            throw std::logic_error("Buffer is empty");
        return *_state;
    }
    BufferBinding ViewBufferBase::binding() const {
        (void)state();
        return BufferBinding(_state);
    }
    bool ViewBufferBase::wait(std::uint64_t timeout) const {
        const auto state = state_owner();
        return state->wait(timeout);
    }
    void ViewBufferBase::resize(size_t size) {
        const auto state = state_owner();
        BufferMutation mutation(state);
        state->require_idle();
        if (size > state->current->size())
            throw std::out_of_range("Imported buffer capacity is fixed");
        state->size = size;
    }
    void ViewBufferBase::write(std::span<const std::byte> bytes, size_t offset) {
        const auto state = state_owner();
        BufferMutation mutation(state);
        state->require_idle();
        if (offset > state->size || bytes.size() > state->size - offset)
            throw std::out_of_range("Buffer write range");
        if (!bytes.empty())
            state->current->write(bytes, offset);
    }
    void ViewBufferBase::assign(std::span<const std::byte> bytes) {
        const auto state = state_owner();
        BufferMutation mutation(state);
        state->require_idle();
        if (bytes.size() > (state->current ? state->current->size() : 0))
            throw std::out_of_range("Imported buffer capacity is fixed");
        if (!bytes.empty())
            state->current->write(bytes);
        state->size = bytes.size();
    }
    void ViewBufferBase::read_into(std::span<std::byte> destination, size_t offset) const {
        const auto state = state_owner();
        BufferMutation mutation(state);
        state->require_idle();
        if (offset > state->size || destination.size() > state->size - offset)
            throw std::out_of_range("Buffer read range");
        if (!destination.empty())
            state->current->read_into(destination, offset);
    }
    AHO_NAMESPACE::command::BufferUpload ViewBufferBase::upload(const Buffer<BufferType::GENERAL>& source, size_t offset) const {
        return AHO_NAMESPACE::command::BufferUpload(binding(), source, offset);
    }
} // namespace AHO_NAMESPACE::resource::detail

namespace AHO_NAMESPACE::resource {
    ImportedBuffer::ImportedBuffer(VSL_NAMESPACE::BufferView storage) : Base(std::move(storage)) {}
    ImportedBuffer import_buffer(VSL_NAMESPACE::BufferView storage) {
        return ImportedBuffer(std::move(storage));
    }
} // namespace AHO_NAMESPACE::resource
