#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/exceptions.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

namespace VSL_NAMESPACE {
    namespace {
        lifetime::OwnedRef<impl::SurfaceImpl> surface_owner(const Surface& value) {
            const auto& owner = StartupAccess::Owned(value);
            StartupAccess::RequireCanonical(owner);
            return owner;
        }
    } // namespace

    Surface::Surface(PureWindow* window, VulkanAccessor instance) {
        if (!window || !*window)
            throw exceptions::RuntimeException("Surface", "A live window is required");
        if (!StartupAccess::Owned(instance) || !StartupAccess::Owned(instance)->instance)
            throw exceptions::RuntimeException("Surface", "A Vulkan instance is required");
        const auto os = window->window_owner();
        _owner = StartupAccess::Owned(StartupAccess::FromShared(lifetime::make_heap_surface(std::move(instance), os)));
    }

    Surface::Surface(PureWindow* window, Surface source) {
        StartupAccess::RequireCanonical(source._owner);
        const auto os = source.get_window_owner();
        if (!window || !*window)
            throw std::invalid_argument("Surface plugin requires a live window");
        const auto target_window = window->window_owner();
        if (!source._owner->surface || source._owner->owner_node->retiring() ||
            source._owner->window != target_window.native_handle() || os.native_handle() != target_window.native_handle())
            throw std::invalid_argument("Surface plugin requires its original live OS window");
        const auto target = target_window.owner_ref();
        const auto original = os.owner_ref();
        if (target.owner_before(original) || original.owner_before(target))
            throw std::invalid_argument("Surface plugin requires its original OS window owner");
        _owner = std::move(source._owner);
    }

    lifetime::OwnerRef Surface::owner_ref() const {
        return surface_owner(*this);
    }

    VulkanAccessor Surface::get_instance() const {
        const auto source = surface_owner(*this);
        const auto parent = source->owner_context ? source->owner_context->instance.lock() : lifetime::OwnedRef<impl::VulkanImplAccessor>{};
        if (!parent.use_count() || parent.get() != source->vulkan)
            throw std::logic_error("Surface has no Instance owner");
        return StartupAccess::FromOwned(parent);
    }

    lifetime::OSWindowOwner Surface::get_window_owner() const {
        const auto source = surface_owner(*this);
        const auto parent = source->owner_context ? source->owner_context->window.lock() : lifetime::OwnedRef<impl::OSWindowImpl>{};
        if (!parent.use_count() || parent.get() != source->os_window)
            throw std::logic_error("Surface has no OS window owner");
        return lifetime::detail::WindowOwnerAccess::From(parent);
    }

    WeakSurface Surface::weak() const noexcept {
        return WeakSurface(_owner.weak());
    }

    Surface WeakSurface::lock() const noexcept {
        return Surface(_owner.lock());
    }
} // namespace VSL_NAMESPACE
