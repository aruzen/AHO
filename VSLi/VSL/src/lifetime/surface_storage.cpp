#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<SurfaceSlotStorage> && offsetof(SurfaceSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<impl::SurfaceImpl>);
    static_assert(std::is_nothrow_default_constructible_v<SurfaceOwnerContext>);
    namespace {
        struct HeapSurfaceOwner {
            DependencyNode node;
            OwnerRef instance;
            OSWindowOwner window;
            alignas(SurfaceSlotStorage) std::byte storage[sizeof(SurfaceSlotStorage)];
            HeapSurfaceOwner(VulkanAccessor instance, OSWindowOwner window);
            ~HeapSurfaceOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        HeapSurfaceOwner::HeapSurfaceOwner(VulkanAccessor instance, OSWindowOwner window) : instance(instance.owner_ref()), window(std::move(window)) {
            const auto& os = detail::WindowOwnerAccess::Ref(this->window);
            if (!os || !os->node.matches(detail::OwnerAccess::Shared(os)) || os->node.retiring())
                throw std::invalid_argument("Surface requires its canonical OS window owner");
            construct_surface(storage, *instance.accessor, *os);
            try {
                set_surface_owners(surface_at(storage), this->instance.alias(instance.accessor.get()), this->window);
            } catch (...) {
                destroy_surface(storage);
                throw;
            }
        }
        HeapSurfaceOwner::~HeapSurfaceOwner() {
            node.retire();
            destroy_surface(storage);
        }
        void HeapSurfaceOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            const auto& root = *static_cast<const HeapSurfaceOwner*>(owner);
            visitor(context, root.instance);
            const auto window = root.window.owner_ref();
            visitor(context, window);
        }
    } // namespace
    void construct_surface(void* storage, impl::VulkanImplAccessor& instance, impl::OSWindowImpl& window, const operation::SurfaceNativeCalls& calls) {
        if (!window.window || !window.glfw)
            throw std::invalid_argument("Surface requires a live managed OS window");
        GLFWCallScope scope;
        auto* slot = std::construct_at(static_cast<SurfaceSlotStorage*>(storage));
        auto* context = std::construct_at(reinterpret_cast<SurfaceOwnerContext*>(slot->context));
        auto* surface = std::construct_at(reinterpret_cast<impl::SurfaceImpl*>(slot->value));
        surface->owner_context = context;
        try {
            operation::initialize_surface(*surface, instance, window.window, calls);
            surface->os_window = &window;
        } catch (...) {
            std::destroy_at(surface);
            std::destroy_at(context);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::SurfaceImpl& surface_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::SurfaceImpl*>(static_cast<SurfaceSlotStorage*>(storage)->value));
    }
    void destroy_surface(void* storage) noexcept {
        auto* context = surface_at(storage).owner_context;
        std::destroy_at(&surface_at(storage));
        std::destroy_at(context);
        std::destroy_at(static_cast<SurfaceSlotStorage*>(storage));
    }
    void set_surface_owners(impl::SurfaceImpl& target, OwnedRef<impl::VulkanImplAccessor> instance, const OSWindowOwner& window) {
        const auto& os = detail::WindowOwnerAccess::Ref(window);
        if (!target.owner_context || !instance.use_count() || instance.get() != target.vulkan || !instance->owner_node ||
            instance->owner_node->retiring() || !instance->owner_node->matches(detail::OwnerAccess::Shared(instance)) ||
            !os || os.get() != target.os_window || os->node.retiring() || !os->node.matches(detail::OwnerAccess::Shared(os)))
            throw std::invalid_argument("Surface getters require their canonical parents");
        target.owner_context->instance = instance.weak();
        target.owner_context->window = os.weak();
    }
    std::shared_ptr<impl::SurfaceImpl> make_heap_surface(VulkanAccessor instance, OSWindowOwner window) {
        auto owner = std::make_shared<HeapSurfaceOwner>(std::move(instance), std::move(window));
        auto* surface = &surface_at(owner->storage);
        owner->node.publish(owner, owner.get(), HeapSurfaceOwner::VisitDependencies);
        surface->owner_node = &owner->node;
        return {std::move(owner), surface};
    }
    void require_surface_owner(const Surface& source) {
        require_surface_owner(source.data_);
    }
    void require_surface_owner(const std::shared_ptr<impl::SurfaceImpl>& value) {
        if (!value || !value.use_count() || !value->surface || !value->owner_node || value->owner_node->retiring() || !value->owner_node->matches(value))
            throw std::invalid_argument("Surface requires its live canonical owner");
    }
} // namespace VSL_NAMESPACE::lifetime
