#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/exceptions.hpp>

namespace VSL_NAMESPACE {
    namespace {
        lifetime::OwnedRef<impl::SurfaceImpl> surface_owner(std::shared_ptr<impl::SurfaceImpl> data) {
            lifetime::require_surface_owner(data);
            const auto* node = data->owner_node;
            return node->ref(std::move(data));
        }
    } // namespace
    Surface::Surface(PureWindow* window, VulkanAccessor instance) {
        if (!window || !window->data_ || !window->data_->window_handle)
            throw exceptions::RuntimeException("Surface", "A live window is required");
        if (!instance.accessor || !instance.accessor->instance)
            throw exceptions::RuntimeException("Surface", "A Vulkan instance is required");
        data_ = lifetime::make_heap_surface(std::move(instance), window->window_owner());
    }
    Surface::Surface(std::shared_ptr<impl::SurfaceImpl> data) : data_(std::move(data)) {}
    Surface::Surface(PureWindow* window, Surface source) {
        const auto owner = source.owner_ref();
        const auto os = source.get_window_owner();
        if (!window || !window->data_ || !window->data_->window_handle || !source.data_->surface || source.data_->owner_node->retiring() ||
            source.data_->window != window->data_->window_handle || os.native_handle() != window->data_->window_handle)
            throw std::invalid_argument("Surface plugin requires its original live OS window");
        const auto target = window->window_owner().owner_ref();
        const auto original = os.owner_ref();
        if (target.owner_before(original) || original.owner_before(target))
            throw std::invalid_argument("Surface plugin requires its original OS window owner");
        data_ = std::move(source.data_);
    }
    lifetime::OwnerRef Surface::owner_ref() const {
        return surface_owner(data_);
    }
    VulkanAccessor Surface::get_instance() const {
        const auto source = surface_owner(data_);
        const auto parent = source->owner_context ? source->owner_context->instance.lock() : lifetime::OwnedRef<impl::VulkanImplAccessor>{};
        if (!parent.use_count() || parent.get() != source->vulkan)
            throw std::logic_error("Surface has no Instance owner");
        return {parent.share()};
    }
    lifetime::OSWindowOwner Surface::get_window_owner() const {
        const auto source = surface_owner(data_);
        const auto parent = source->owner_context ? source->owner_context->window.lock() : lifetime::OwnedRef<impl::OSWindowImpl>{};
        if (!parent.use_count() || parent.get() != source->os_window)
            throw std::logic_error("Surface has no OS window owner");
        return lifetime::detail::WindowOwnerAccess::From(parent);
    }
} // namespace VSL_NAMESPACE
