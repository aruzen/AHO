#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/buffer_view.hpp>
#include <VSL/operation/buffer_ops.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <cstring>

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
}

namespace VSL_NAMESPACE {
    BufferIdentity BufferRef::identity() const {
        return lifetime::detail::IdentityAccess::From<BufferIdentity>(_data);
    }
    size_t BufferRef::size() const {
        if (!_data)
            throw std::logic_error("Buffer reference is empty");
        return _data->requirement_size;
    }
    MemoryType BufferRef::usage() const {
        if (!_data)
            throw std::logic_error("Buffer reference is empty");
        return _data->usage;
    }
    ResourceUseState& BufferRef::use() const {
        if (!_data || !_data->use)
            throw std::logic_error("Buffer reference has no use state");
        return *_data->use;
    }
    MemoryProperty BufferRef::properties() const {
        if (!_data)
            throw std::logic_error("Buffer reference is empty");
        return _data->properties;
    }
    LogicalDeviceAccessor BufferRef::device() const {
        if (!_data || !_data->device)
            throw std::logic_error("Buffer reference has no device");
        return lifetime::device_from_impl(*_data->device);
    }
    void BufferRef::fill(std::byte value) const {
        const auto ref = *this;
        ref.use().require_idle();
        auto* destination = operation::map_buffer(*ref._data, 0, ref.size());
        std::memset(destination, std::to_integer<int>(value), ref.size());
        operation::synchronize_buffer(*ref._data, 0, ref.size(), true);
    }
    void BufferRef::write(std::span<const std::byte> bytes, size_t offset) const {
        const auto ref = *this;
        ref.use().require_idle();
        auto* destination = operation::map_buffer(*ref._data, offset, bytes.size());
        if (!bytes.empty())
            std::memmove(destination, bytes.data(), bytes.size());
        operation::synchronize_buffer(*ref._data, offset, bytes.size(), true);
    }
    void BufferRef::read_into(std::span<std::byte> destination, size_t offset) const {
        const auto ref = *this;
        ref.use().require_idle();
        auto* source = operation::map_buffer(*ref._data, offset, destination.size());
        operation::synchronize_buffer(*ref._data, offset, destination.size(), false);
        if (!destination.empty())
            std::memmove(destination.data(), source, destination.size());
    }
    BufferView::BufferView(BufferRef ref, lifetime::OwnerRef owner, size_t offset, size_t bytes)
        : _ref(ref), _owner(std::move(owner)), _offset(offset), _bytes(bytes) {
        if (!_owner.use_count())
            throw std::invalid_argument("Buffer view requires an owner");
        if (!bytes || offset > ref.size() || bytes > ref.size() - offset)
            throw std::out_of_range("Buffer view range");
        if (!_owner.managed())
            _owner = lifetime::detail::owned_ref(_owner.share(), lifetime::detail::BufferAccess::Impl(ref)->owner_node);
    }
    BufferView& BufferView::operator=(BufferView replacement) noexcept {
        swap(replacement);
        return *this;
    }
    void BufferView::swap(BufferView& other) noexcept {
        std::swap(_ref, other._ref);
        _owner.swap(other._owner);
        std::swap(_offset, other._offset);
        std::swap(_bytes, other._bytes);
    }
    BufferView BufferView::Create(LogicalDeviceAccessor device, const BufferCreateInfo& info) {
        auto data = lifetime::make_heap_buffer(std::move(device), info);
        const auto ref = BufferAccess::Ref(data.get());
        const auto* node = data->owner_node;
        auto owner = lifetime::detail::owned_ref(std::move(data), node);
        return {ref, std::move(owner), 0, info.bytes};
    }
    BufferView BufferView::FromBuffer(const BufferAccessor& buffer) {
        const auto owner = BufferAccess::Owned(buffer);
        if (!owner || !owner.use_count())
            throw std::invalid_argument("Buffer view requires an owning buffer");
        return {BufferAccess::Ref(owner.get()), owner, 0, owner->requirement_size};
    }
    BufferView BufferView::view(size_t offset, size_t bytes) const {
        if (!bytes || offset > _bytes || bytes > _bytes - offset)
            throw std::out_of_range("Buffer subview range");
        return {_ref, _owner, _offset + offset, bytes};
    }
    void BufferView::write(std::span<const std::byte> bytes, size_t offset) const {
        const auto view = *this;
        if (!view._owner.use_count())
            throw std::logic_error("Buffer view is empty");
        if (offset > view._bytes || bytes.size() > view._bytes - offset)
            throw std::out_of_range("Buffer view write range");
        view._ref.write(bytes, view._offset + offset);
    }
    void BufferView::read_into(std::span<std::byte> destination, size_t offset) const {
        const auto view = *this;
        if (!view._owner.use_count())
            throw std::logic_error("Buffer view is empty");
        if (offset > view._bytes || destination.size() > view._bytes - offset)
            throw std::out_of_range("Buffer view read range");
        view._ref.read_into(destination, view._offset + offset);
    }
    ResourceUse BufferView::resource_use(ResourceUse constraint) const {
        const auto view = *this;
        if (!view._owner.use_count())
            throw std::logic_error("Buffer view is empty");
        return ResourceUse{view._owner.alias(&view._ref.use()), view._owner}.with_constraint(std::move(constraint));
    }
} // namespace VSL_NAMESPACE
