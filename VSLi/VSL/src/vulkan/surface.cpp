#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/exceptions.hpp>

namespace VSL_NAMESPACE {
    namespace {
        lifetime::OwnedRef<impl::SurfaceImpl> surface_owner(std::shared_ptr<impl::SurfaceImpl> data) {
            if (!data || !data.use_count() || !data->owner_node || !data->owner_node->matches(data))
                throw std::invalid_argument("Surface requires its canonical owner");
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
