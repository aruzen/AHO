#pragma once
#include "../define.hpp"
#include "pv.hpp"
#include "../utils/string.hpp"

#include "../window.hpp"
#include "vulkan.hpp"

namespace VSL_NAMESPACE {
    class WeakSurface;

    struct Surface : public VSL_NAMESPACE::PureWindow::Plugin {
    private:
        lifetime::OwnedRef<impl::SurfaceImpl> _owner;

        explicit Surface(lifetime::OwnedRef<impl::SurfaceImpl> owner) noexcept : _owner(std::move(owner)) {}
        friend struct lifetime::detail::StartupAccess;
        friend class WeakSurface;

    public:
        Surface() = default;
        Surface(const Surface&) = default;
        Surface(Surface&&) noexcept = default;

        Surface& operator=(Surface value) noexcept {
            swap(value);
            return *this;
        }

        // 登録IDはWindowの登録表に属し、Native ownerの交換では移動しない。
        void swap(Surface& other) noexcept { _owner.swap(other._owner); }
        explicit operator bool() const noexcept { return bool(_owner); }

        Surface(VSL_NAMESPACE::PureWindow* data, VSL_NAMESPACE::VulkanAccessor instance);
        // 移行用plugin登録。sourceのnative Surfaceとownerをそのまま使う。
        Surface(VSL_NAMESPACE::PureWindow* window, Surface source);
        lifetime::OwnerRef owner_ref() const;
        VulkanAccessor get_instance() const;
        lifetime::OSWindowOwner get_window_owner() const;
        WeakSurface weak() const noexcept;
    };

    class WeakSurface {
        lifetime::WeakOwnedRef<impl::SurfaceImpl> _owner;

        explicit WeakSurface(lifetime::WeakOwnedRef<impl::SurfaceImpl> owner) noexcept : _owner(std::move(owner)) {}
        friend struct Surface;

    public:
        WeakSurface() = default;
        WeakSurface(const WeakSurface&) = default;
        WeakSurface(WeakSurface&&) noexcept = default;

        WeakSurface& operator=(WeakSurface value) noexcept {
            swap(value);
            return *this;
        }

        void swap(WeakSurface& other) noexcept { _owner.swap(other._owner); }
        void reset() noexcept { _owner.reset(); }
        bool expired() const noexcept { return _owner.expired(); }

        Surface lock() const noexcept;
    };
} // namespace VSL_NAMESPACE
