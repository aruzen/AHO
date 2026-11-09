#pragma once
#include "../operation/buffer_create_info.hpp"
#include "resource_use.hpp"
#include "resource_identity.hpp"
#include <stdexcept>

namespace VSL_NAMESPACE {
    namespace lifetime::detail {
        struct BufferAccess;
    }
    // 非所有。所有者または構築scopeが有効な間だけ利用する。
    struct BufferRef {
    private:
        friend struct lifetime::detail::BufferAccess;
        impl::BufferImpl* _data = nullptr;
        explicit BufferRef(impl::BufferImpl* data) : _data(data) {}

    public:
        BufferRef() = default;
        BufferIdentity identity() const;
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
        friend struct lifetime::detail::BufferAccess;
        BufferRef _ref;
        lifetime::OwnerRef _owner;
        size_t _offset = 0, _bytes = 0;

        BufferView(BufferRef ref, lifetime::OwnerRef owner, size_t offset, size_t bytes);

    public:
        BufferView(const BufferView&) = default;
        BufferView(BufferView&&) noexcept = default;
        BufferView& operator=(BufferView replacement) noexcept;
        void swap(BufferView& other) noexcept;
        // 単独heap所有で生成する。CPU arenaの配置計画は使用しない。
        static BufferView Create(LogicalDeviceAccessor device, const BufferCreateInfo& info);
        static BufferView FromBuffer(const BufferAccessor& buffer);
        BufferRef ref() const& {
            if (!_owner.use_count())
                throw std::logic_error("Buffer view is empty");
            return _ref;
        }
        BufferRef ref() const&& = delete;
        BufferIdentity identity() const { return _ref.identity(); }
        std::shared_ptr<void> owner() const { return _owner.share(); }
        lifetime::OwnerRef owner_ref() const { return _owner; }
        size_t byte_offset() const { return _offset; }
        size_t size() const { return _bytes; }
        BufferView view(size_t offset, size_t bytes) const;
        void write(std::span<const std::byte> bytes, size_t offset = 0) const;
        void read_into(std::span<std::byte> destination, size_t offset = 0) const;
        ResourceUse resource_use(ResourceUse constraint = {}) const;
    };
} // namespace VSL_NAMESPACE
