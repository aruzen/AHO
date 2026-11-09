#pragma once
#include <VSL/lifetime/buffer_view.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    // raw実体との接続は非公開の構築・操作処理だけで使う。
    struct BufferAccess {
        static BufferRef Ref(impl::BufferImpl* data) { return BufferRef(data); }
        static impl::BufferImpl* Impl(BufferRef ref) { return ref._data; }
        static BufferView View(BufferRef ref, OwnerRef owner, size_t offset, size_t bytes) {
            return BufferView(ref, std::move(owner), offset, bytes);
        }
        static OwnedRef<impl::BufferImpl> Owned(const BufferView& view) {
            return view._owner.alias(view._ref._data);
        }
    };
} // namespace VSL_NAMESPACE::lifetime::detail
