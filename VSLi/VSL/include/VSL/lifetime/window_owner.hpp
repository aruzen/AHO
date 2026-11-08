#pragma once
#include "owner_ref.hpp"
#include <string_view>

namespace VSL_NAMESPACE::impl {
    struct OSWindowImpl;
}
namespace VSL_NAMESPACE::lifetime {
    namespace detail {
        struct WindowOwnerAccess;
    }
    // plugins／WindowData／Engineを所有しないnative windowの寿命境界。
    class OSWindowOwner {
        friend struct detail::WindowOwnerAccess;
        OwnedRef<impl::OSWindowImpl> _window;
        explicit OSWindowOwner(OwnedRef<impl::OSWindowImpl> window) : _window(std::move(window)) {}

    public:
        OSWindowOwner() = default;
        static OSWindowOwner Create(std::string_view name, int width, int height);
        void* native_handle() const& noexcept;
        void* native_handle() const&& = delete;
        OwnerRef owner_ref() const { return _window; }
        explicit operator bool() const noexcept { return bool(_window); }
    };
} // namespace VSL_NAMESPACE::lifetime
