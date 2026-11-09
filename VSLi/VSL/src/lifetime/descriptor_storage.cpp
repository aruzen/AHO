#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>
#include <algorithm>
#include <array>
#include <limits>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::ResourceBindingLayoutImpl>);
    static_assert(std::is_nothrow_default_constructible_v<impl::GraphicResourcePoolImpl>);
    static_assert(std::is_nothrow_default_constructible_v<impl::GraphicResourceImpl>);
    namespace {
        struct HeapDescriptorLayoutOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(DescriptorLayoutSlotStorage) std::byte storage[sizeof(DescriptorLayoutSlotStorage)];
            HeapDescriptorLayoutOwner(LogicalDeviceAccessor device, const DescriptorLayoutCreateInfo& info);
            ~HeapDescriptorLayoutOwner();
        };
        struct HeapDescriptorPoolOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(DescriptorPoolSlotStorage) std::byte storage[sizeof(DescriptorPoolSlotStorage)];
            HeapDescriptorPoolOwner(LogicalDeviceAccessor device, const DescriptorPoolCreateInfo& info);
            ~HeapDescriptorPoolOwner();
        };
        struct HeapDescriptorSetOwner {
            std::vector<OwnerRef> dependencies;
            DescriptorOwnerContext context;
            alignas(DescriptorSetSlotStorage) std::byte storage[sizeof(DescriptorSetSlotStorage)];
            HeapDescriptorSetOwner(OwnedRef<impl::GraphicResourcePoolImpl> pool, OwnedRef<impl::ResourceBindingLayoutImpl> layout);
            ~HeapDescriptorSetOwner();
            void publish(const std::shared_ptr<HeapDescriptorSetOwner>& owner);
            static void VisitDependencies(const void* input, DependencyNode::EdgeVisitor visitor, void* context);
        };
        HeapDescriptorLayoutOwner::HeapDescriptorLayoutOwner(LogicalDeviceAccessor device, const DescriptorLayoutCreateInfo& info) : device(device) {
            require_device_owner(device);
            construct_descriptor_layout(storage, *device.data_, info);
        }
        HeapDescriptorLayoutOwner::~HeapDescriptorLayoutOwner() {
            node.retire();
            destroy_descriptor_layout(storage);
        }
        HeapDescriptorPoolOwner::HeapDescriptorPoolOwner(LogicalDeviceAccessor device, const DescriptorPoolCreateInfo& info) : device(device) {
            require_device_owner(device);
            construct_descriptor_pool(storage, *device.data_, info);
        }
        HeapDescriptorPoolOwner::~HeapDescriptorPoolOwner() {
            node.retire();
            destroy_descriptor_pool(storage);
        }
        HeapDescriptorSetOwner::HeapDescriptorSetOwner(OwnedRef<impl::GraphicResourcePoolImpl> pool,
                                                       OwnedRef<impl::ResourceBindingLayoutImpl> layout) {
            if (!pool || !pool.use_count() || !layout || !layout.use_count())
                throw std::invalid_argument("Descriptor allocation requires owning pool and layout");
            dependencies.reserve(2);
            detail::retain_owner(dependencies, pool);
            detail::retain_owner(dependencies, layout);
            context.dependencies = dependencies;
            context.dependency_storage = &dependencies;
            context.policy = DependencyPolicy::DYNAMIC;
            context.single_resource = true;
            context.device_parent = pool->device->owner_node;
            prepare_descriptor_set(storage, *pool, *layout);
        }
        HeapDescriptorSetOwner::~HeapDescriptorSetOwner() {
            context.node.retire();
            destroy_descriptor_set(storage);
        }
        void HeapDescriptorSetOwner::publish(const std::shared_ptr<HeapDescriptorSetOwner>& owner) {
            context.node.publish(owner, this, VisitDependencies);
            auto& resource = descriptor_set_at(storage);
            resource.owner_context = &context;
            resource.owner_node = &context.node;
        }
        void HeapDescriptorSetOwner::VisitDependencies(const void* input, DependencyNode::EdgeVisitor visitor, void* output) {
            const auto& root = *static_cast<const HeapDescriptorSetOwner*>(input);
            for (const auto& owner : root.context.dependencies)
                visitor(output, owner);
            visit_descriptor_edges(descriptor_set_at(root.storage), visitor, output);
        }
    } // namespace

    void construct_descriptor_layout(void* storage, LogicalDeviceAccessor device, const DescriptorLayoutCreateInfo& info) {
        require_device_owner(device);
        construct_descriptor_layout(storage, *device.data_, info);
    }
    void construct_descriptor_layout(void* storage, impl::LogicalDeviceImpl& device, const DescriptorLayoutCreateInfo& info) {
        if (!device.device)
            throw std::invalid_argument("Descriptor layout requires a live device");
        auto* slot = std::construct_at(static_cast<DescriptorLayoutSlotStorage*>(storage));
        auto* layout = std::construct_at(reinterpret_cast<impl::ResourceBindingLayoutImpl*>(slot->value));
        try {
            operation::initialize_descriptor_layout(*layout, device, info);
        } catch (...) {
            std::destroy_at(layout);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::ResourceBindingLayoutImpl& descriptor_layout_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::ResourceBindingLayoutImpl*>(static_cast<DescriptorLayoutSlotStorage*>(storage)->value));
    }
    void destroy_descriptor_layout(void* storage) noexcept {
        std::destroy_at(&descriptor_layout_at(storage));
        std::destroy_at(static_cast<DescriptorLayoutSlotStorage*>(storage));
    }
    void construct_descriptor_pool(void* storage, LogicalDeviceAccessor device, const DescriptorPoolCreateInfo& info) {
        require_device_owner(device);
        construct_descriptor_pool(storage, *device.data_, info);
    }
    void construct_descriptor_pool(void* storage, impl::LogicalDeviceImpl& device, const DescriptorPoolCreateInfo& info) {
        if (!device.device)
            throw std::invalid_argument("Descriptor pool requires a live device");
        auto* slot = std::construct_at(static_cast<DescriptorPoolSlotStorage*>(storage));
        auto* pool = std::construct_at(reinterpret_cast<impl::GraphicResourcePoolImpl*>(slot->value));
        try {
            operation::initialize_descriptor_pool(*pool, device, info);
        } catch (...) {
            std::destroy_at(pool);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::GraphicResourcePoolImpl& descriptor_pool_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::GraphicResourcePoolImpl*>(static_cast<DescriptorPoolSlotStorage*>(storage)->value));
    }
    void destroy_descriptor_pool(void* storage) noexcept {
        std::destroy_at(&descriptor_pool_at(storage));
        std::destroy_at(static_cast<DescriptorPoolSlotStorage*>(storage));
    }
    void prepare_descriptor_set(void* storage, impl::GraphicResourcePoolImpl& pool, impl::ResourceBindingLayoutImpl& layout) {
        if (!pool.descriptor_pool || pool.retiring)
            throw std::logic_error("Descriptor pool has been destroyed");
        if (!layout.layout || layout.device != pool.device)
            throw std::invalid_argument("Descriptor layout device mismatch");
        auto* slot = std::construct_at(static_cast<DescriptorSetSlotStorage*>(storage));
        auto* resource = std::construct_at(reinterpret_cast<impl::GraphicResourceImpl*>(slot->value));
        resource->pool = &pool;
        resource->layout = &layout;
        resource->allocation_layout = &layout;
        resource->native_device = pool.native_device;
    }
    void construct_descriptor_set(void* storage, impl::GraphicResourcePoolImpl& pool, impl::ResourceBindingLayoutImpl& layout) {
        prepare_descriptor_set(storage, pool, layout);
        try {
            const std::array sets{&descriptor_set_at(storage)};
            const auto result = operation::allocate_descriptor_sets(pool, sets);
            if (result != vk::Result::eSuccess)
                throw std::runtime_error("Descriptor allocation failed: " + vk::to_string(result));
        } catch (...) {
            destroy_descriptor_set(storage);
            throw;
        }
    }
    impl::GraphicResourceImpl& descriptor_set_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::GraphicResourceImpl*>(static_cast<DescriptorSetSlotStorage*>(storage)->value));
    }
    const impl::GraphicResourceImpl& descriptor_set_at(const void* storage) {
        return *std::launder(reinterpret_cast<const impl::GraphicResourceImpl*>(static_cast<const DescriptorSetSlotStorage*>(storage)->value));
    }
    void destroy_descriptor_set(void* storage) noexcept {
        std::destroy_at(&descriptor_set_at(storage));
        std::destroy_at(static_cast<DescriptorSetSlotStorage*>(storage));
    }
    std::shared_ptr<impl::ResourceBindingLayoutImpl> make_heap_descriptor_layout(LogicalDeviceAccessor device, const DescriptorLayoutCreateInfo& info) {
        auto owner = std::make_shared<HeapDescriptorLayoutOwner>(device, info);
        auto* layout = &descriptor_layout_at(owner->storage);
        return detail::publish_device_leaf(std::move(owner), layout);
    }
    std::shared_ptr<impl::GraphicResourcePoolImpl> make_heap_descriptor_pool(LogicalDeviceAccessor device, const DescriptorPoolCreateInfo& info) {
        auto owner = std::make_shared<HeapDescriptorPoolOwner>(device, info);
        auto* pool = &descriptor_pool_at(owner->storage);
        pool->owner = owner;
        return detail::publish_device_leaf(std::move(owner), pool);
    }
    std::pair<vk::Result, std::vector<std::shared_ptr<impl::GraphicResourceImpl>>> make_heap_descriptor_sets(
        OwnedRef<impl::GraphicResourcePoolImpl> pool, std::span<const OwnedRef<impl::ResourceBindingLayoutImpl>> layouts,
        DescriptorSetAllocator allocate) {
        if (!allocate)
            throw std::invalid_argument("Descriptor allocation operation is empty");
        if (!pool || !pool->descriptor_pool || pool->retiring)
            throw std::logic_error("Descriptor pool has been destroyed");
        if (layouts.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Descriptor allocation count exceeds uint32_t");
        std::erase_if(pool->resources, [](const auto& resource) { return resource.expired(); });
        std::vector<std::shared_ptr<impl::GraphicResourceImpl>> resources;
        std::vector<impl::GraphicResourceImpl*> prepared;
        resources.reserve(layouts.size());
        prepared.reserve(layouts.size());
        if (layouts.size() > pool->resources.max_size() - pool->resources.size())
            throw std::length_error("Descriptor resource registry capacity exceeded");
        pool->resources.reserve(pool->resources.size() + layouts.size());
        for (const auto& layout : layouts) {
            auto owner = std::make_shared<HeapDescriptorSetOwner>(pool, layout);
            auto* resource = &descriptor_set_at(owner->storage);
            owner->publish(owner);
            resources.emplace_back(std::move(owner), resource);
            prepared.push_back(resource);
        }
        const auto result = prepared.empty() ? vk::Result::eSuccess : allocate(*pool, prepared);
        if (result != vk::Result::eSuccess)
            return {result, {}};
        for (const auto& resource : resources)
            pool->resources.push_back(resource);
        return {result, std::move(resources)};
    }
} // namespace VSL_NAMESPACE::lifetime

namespace VSL_NAMESPACE::impl {
    ResourceBindingLayoutImpl::~ResourceBindingLayoutImpl() {
        operation::destroy_descriptor_layout(*this);
    }
    GraphicResourcePoolImpl::~GraphicResourcePoolImpl() {
        operation::destroy_descriptor_pool(*this);
    }
    GraphicResourceImpl::~GraphicResourceImpl() {
        operation::destroy_descriptor_set(*this);
    }
} // namespace VSL_NAMESPACE::impl
