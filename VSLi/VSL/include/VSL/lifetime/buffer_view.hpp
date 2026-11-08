#pragma once
#include "../operation/buffer_create_info.hpp"
#include "resource_use.hpp"

namespace VSL_NAMESPACE {
    // 非所有。所有者または構築scopeが有効な間だけ利用する。
    struct BufferRef {
        impl::BufferImpl* data = nullptr;
        size_t size() const;
        MemoryType usage() const;
        MemoryProperty properties() const;
        LogicalDeviceAccessor device() const;
        ResourceUseState& use() const;
        void write(std::span<const std::byte> bytes, size_t offset = 0) const;
        void read_into(std::span<std::byte> destination, size_t offset = 0) const;
        void fill(std::byte value) const;
    };

    // 公開viewはstorageを保持する。内部slotにこのowning viewを格納しない。
    class BufferView {
        BufferRef _ref;
        lifetime::OwnerRef _owner;
        size_t _offset = 0, _bytes = 0;

    public:
        BufferView(BufferRef ref, lifetime::OwnerRef owner, size_t offset, size_t bytes);
        static BufferView FromBuffer(const BufferAccessor& buffer);
        BufferRef ref() const { return _ref; }
        std::shared_ptr<void> owner() const { return _owner.share(); }
        lifetime::OwnerRef owner_ref() const { return _owner; }
        size_t byte_offset() const { return _offset; }
        size_t size() const { return _bytes; }
        BufferView view(size_t offset, size_t bytes) const;
        void write(std::span<const std::byte> bytes, size_t offset = 0) const;
        void read_into(std::span<std::byte> destination, size_t offset = 0) const;
        // 既存Typed/native bindingへ接続する移行用adapter。
        std::shared_ptr<impl::BufferImpl> legacy_owner() const { return _owner.alias(_ref.data).share(); }
        ResourceUse resource_use(ResourceUse constraint = {}) const { return ResourceUse{_owner.alias(&_ref.use()), _owner}.with_constraint(std::move(constraint)); }
    };
} // namespace VSL_NAMESPACE
