#include <VSL/vulkan/buffer_view.hpp>
#include <VSL/backend/buffer_ops.hpp>
#include <cstring>

namespace VSL_NAMESPACE {
    size_t BufferRef::size() const {
        if (!data)
            throw std::logic_error("Buffer reference is empty");
        return data->requirement_size;
    }
    MemoryType BufferRef::usage() const {
        if (!data)
            throw std::logic_error("Buffer reference is empty");
        return data->usage;
    }
    ResourceUseState& BufferRef::use() const {
        if (!data || !data->use)
            throw std::logic_error("Buffer reference has no use state");
        return *data->use;
    }
    void BufferRef::write(std::span<const std::byte> bytes, size_t offset) const {
        use().require_idle();
        auto* destination = backend::map_buffer(*data, offset, bytes.size());
        if (!bytes.empty())
            std::memmove(destination, bytes.data(), bytes.size());
        backend::synchronize_buffer(*data, offset, bytes.size(), true);
    }
    std::span<const std::byte> BufferRef::read(size_t bytes, size_t offset) const {
        use().require_idle();
        auto* source = backend::map_buffer(*data, offset, bytes);
        backend::synchronize_buffer(*data, offset, bytes, false);
        return {static_cast<const std::byte*>(source), bytes};
    }
    BufferView::BufferView(BufferRef ref, std::shared_ptr<void> owner, size_t offset, size_t bytes)
        : _ref(ref), _owner(std::move(owner)), _offset(offset), _bytes(bytes) {
        if (!_owner.use_count())
            throw std::invalid_argument("Buffer view requires an owner");
        if (!bytes || offset > ref.size() || bytes > ref.size() - offset)
            throw std::out_of_range("Buffer view range");
    }
    BufferView BufferView::FromBuffer(const BufferAccessor& buffer) {
        return {{buffer.data_.get()}, buffer.data_, 0, buffer.requested_size()};
    }
    BufferView BufferView::view(size_t offset, size_t bytes) const {
        if (!bytes || offset > _bytes || bytes > _bytes - offset)
            throw std::out_of_range("Buffer subview range");
        return {_ref, _owner, _offset + offset, bytes};
    }
    void BufferView::write(std::span<const std::byte> bytes, size_t offset) const {
        if (offset > _bytes || bytes.size() > _bytes - offset)
            throw std::out_of_range("Buffer view write range");
        _ref.write(bytes, _offset + offset);
    }
    std::span<const std::byte> BufferView::read(size_t bytes, size_t offset) const {
        if (offset > _bytes || bytes > _bytes - offset)
            throw std::out_of_range("Buffer view read range");
        return _ref.read(bytes, _offset + offset);
    }
} // namespace VSL_NAMESPACE
