#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/lifetime/instance_storage.hpp>
#include <VSL/lifetime/physical_device_storage.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/lifetime/device_storage.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/storage/cpu_region.hpp>
#include <algorithm>

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

namespace VSL_NAMESPACE::lifetime::detail {
    using StartupReference = std::variant<impl::VulkanImplAccessor*, impl::PhysicalDeviceImpl*, impl::SurfaceImpl*, impl::LogicalDeviceImpl*>;
    struct StartupResource {
        storage::StartupPlacement placement;
        StartupReference reference;
        size_t owner_index = storage::StartupPlacement::NO_SLOT;
    };
    struct StartupArenaOwner {
        DependencyNode node;
        std::shared_ptr<storage::detail::CPURegion> memory;
        std::vector<StartupResource> resources;
        std::vector<OwnerRef> dependencies;
        std::vector<size_t> constructed;
        bool claimed = false;
        StartupArenaOwner(std::span<const storage::StartupPlacement> placements, std::span<const StartupInit> inputs,
                          std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment);
        ~StartupArenaOwner();
        void preflight(std::span<const StartupInit> inputs);
        template<typename Facade>
        void preflight_source(const std::variant<typed::ResourceId, Facade>& source, storage::StartupKind kind, bool permanent = true);
        size_t retain(OwnerRef owner);
        void construct(size_t index, const StartupInit& input);
        void clear() noexcept;
        void* slot(const StartupResource& resource) const;
        const StartupResource& find(typed::ResourceId id, storage::StartupKind kind) const;
        OwnerRef resource_owner(const StartupResource& resource) const;
        impl::VulkanImplAccessor& resolve(const InstanceSource& source) const;
        impl::PhysicalDeviceImpl& resolve(const PhysicalDeviceSource& source) const;
        impl::SurfaceImpl& resolve(const SurfaceSource& source) const;
        template<typename Facade>
        OwnerRef source_owner(const std::variant<typed::ResourceId, Facade>& source, storage::StartupKind kind) const;
        void publish(const std::shared_ptr<StartupArenaOwner>& owner, std::span<const StartupInit> inputs);
        static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
    };
    bool initializer_matches(const StartupInit& input, storage::StartupKind kind, bool external);
    OwnerRef checked_owner(const VulkanAccessor& value);
    OwnerRef checked_owner(const PhysicalDevice& value);
    OwnerRef checked_owner(const Surface& value);
    OwnerRef checked_owner(const LogicalDeviceAccessor& value);

    bool initializer_matches(const StartupInit& input, storage::StartupKind kind, bool external) {
        switch (kind) {
            case storage::StartupKind::INSTANCE:
                return external ? std::holds_alternative<VulkanAccessor>(input) : std::holds_alternative<InstanceCreateInfo>(input);
            case storage::StartupKind::PHYSICAL_DEVICE:
                return external ? std::holds_alternative<PhysicalDevice>(input) : std::holds_alternative<PhysicalDeviceInit>(input);
            case storage::StartupKind::SURFACE:
                return external ? std::holds_alternative<Surface>(input) : std::holds_alternative<SurfaceInit>(input);
            case storage::StartupKind::DEVICE:
                return external ? std::holds_alternative<LogicalDeviceAccessor>(input) : std::holds_alternative<DeviceInit>(input) || std::holds_alternative<GraphicsDeviceInit>(input);
        }
        return false;
    }
    OwnerRef checked_owner(const VulkanAccessor& value) {
        auto owner = value.owner_ref();
        if (!StartupAccess::Owned(value)->instance || OwnerAccess::Node(owner)->retiring())
            throw std::invalid_argument("Startup input requires a live Instance");
        return owner;
    }
    OwnerRef checked_owner(const PhysicalDevice& value) {
        auto owner = value.owner_ref();
        if (!StartupAccess::Owned(value)->device || OwnerAccess::Node(owner)->retiring())
            throw std::invalid_argument("Startup input requires a live PhysicalDevice");
        return owner;
    }
    OwnerRef checked_owner(const Surface& value) {
        auto owner = value.owner_ref();
        if (!value.data_->surface || OwnerAccess::Node(owner)->retiring())
            throw std::invalid_argument("Startup input requires a live Surface");
        return owner;
    }
    OwnerRef checked_owner(const LogicalDeviceAccessor& value) {
        require_device_owner(value);
        return value.owner_ref();
    }
    StartupArenaOwner::StartupArenaOwner(std::span<const storage::StartupPlacement> placements, std::span<const StartupInit> inputs,
                                         std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment)
        : memory(std::move(memory)) {
        if (!this->memory || placements.size() != inputs.size())
            throw std::invalid_argument("Startup initializers must match every plan entry");
        resources.reserve(placements.size());
        constructed.reserve(placements.size());
        for (const auto& placement : placements)
            resources.push_back({placement, {static_cast<impl::VulkanImplAccessor*>(nullptr)}});
        preflight(inputs);
        this->memory->claim(bytes, alignment);
        claimed = true;
        try {
            // 配置の宣言順によらず親を先に構築する。
            for (const auto kind : {storage::StartupKind::INSTANCE, storage::StartupKind::PHYSICAL_DEVICE,
                                    storage::StartupKind::SURFACE, storage::StartupKind::DEVICE})
                for (size_t i = 0; i < resources.size(); ++i)
                    if (!resources[i].placement.external && resources[i].placement.kind == kind) {
                        construct(i, inputs[i]);
                        constructed.push_back(i);
                    }
        } catch (...) {
            clear();
            throw;
        }
    }
    StartupArenaOwner::~StartupArenaOwner() {
        node.retire();
        clear();
    }
    size_t StartupArenaOwner::retain(OwnerRef owner) {
        for (size_t i = 0; i < dependencies.size(); ++i)
            if (!owner.owner_before(dependencies[i]) && !dependencies[i].owner_before(owner))
                return i;
        dependencies.push_back(std::move(owner));
        return dependencies.size() - 1;
    }
    template<typename Facade>
    void StartupArenaOwner::preflight_source(const std::variant<typed::ResourceId, Facade>& source, storage::StartupKind kind, bool permanent) {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            (void)find(*id, kind);
        else {
            auto owner = checked_owner(std::get<Facade>(source));
            if (permanent)
                retain(std::move(owner));
        }
    }
    void StartupArenaOwner::preflight(std::span<const StartupInit> inputs) {
        for (size_t i = 0; i < resources.size(); ++i) {
            auto& resource = resources[i];
            const auto& input = inputs[i];
            if (!initializer_matches(input, resource.placement.kind, resource.placement.external))
                throw std::invalid_argument("Startup initializer kind or ownership does not match its plan entry");
            if (resource.placement.external) {
                std::visit([&](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::same_as<Value, VulkanAccessor> || std::same_as<Value, PhysicalDevice>) {
                        resource.owner_index = retain(checked_owner(value));
                        resource.reference = StartupAccess::Owned(value).get();
                    } else if constexpr (std::same_as<Value, Surface> || std::same_as<Value, LogicalDeviceAccessor>) {
                        resource.owner_index = retain(checked_owner(value));
                        resource.reference = value.data_.get();
                    }
                },
                           input);
            }
        }
        for (const auto& input : inputs) {
            if (const auto* physical = std::get_if<PhysicalDeviceInit>(&input))
                preflight_source(physical->instance, storage::StartupKind::INSTANCE);
            else if (const auto* surface = std::get_if<SurfaceInit>(&input)) {
                preflight_source(surface->instance, storage::StartupKind::INSTANCE);
                const auto& window = WindowOwnerAccess::Ref(surface->window);
                if (!window || !window->window || !window->glfw || window->node.retiring() || !window->node.matches(OwnerAccess::Shared(window)))
                    throw std::invalid_argument("Startup Surface requires a live canonical OS window");
                retain(surface->window.owner_ref());
            } else if (const auto* device = std::get_if<DeviceInit>(&input))
                preflight_source(device->physical, storage::StartupKind::PHYSICAL_DEVICE);
            else if (const auto* graphics = std::get_if<GraphicsDeviceInit>(&input)) {
                preflight_source(graphics->physical, storage::StartupKind::PHYSICAL_DEVICE);
                preflight_source(graphics->surface, storage::StartupKind::SURFACE, false);
            }
        }
    }
    void* StartupArenaOwner::slot(const StartupResource& resource) const {
        return memory->base + resource.placement.offset;
    }
    const StartupResource& StartupArenaOwner::find(typed::ResourceId id, storage::StartupKind kind) const {
        const auto found = std::ranges::find(resources, id, [](const auto& resource) { return resource.placement.resource; });
        if (found == resources.end())
            throw std::out_of_range("Unknown startup resource ID");
        if (found->placement.kind != kind)
            throw std::invalid_argument("Startup resource kind mismatch");
        return *found;
    }
    OwnerRef StartupArenaOwner::resource_owner(const StartupResource& resource) const {
        return resource.placement.external ? dependencies.at(resource.owner_index) : node.owner_ref();
    }
    impl::VulkanImplAccessor& StartupArenaOwner::resolve(const InstanceSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return *std::get<impl::VulkanImplAccessor*>(find(*id, storage::StartupKind::INSTANCE).reference);
        return *StartupAccess::Owned(std::get<VulkanAccessor>(source));
    }
    impl::PhysicalDeviceImpl& StartupArenaOwner::resolve(const PhysicalDeviceSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return *std::get<impl::PhysicalDeviceImpl*>(find(*id, storage::StartupKind::PHYSICAL_DEVICE).reference);
        return *StartupAccess::Owned(std::get<PhysicalDevice>(source));
    }
    impl::SurfaceImpl& StartupArenaOwner::resolve(const SurfaceSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return *std::get<impl::SurfaceImpl*>(find(*id, storage::StartupKind::SURFACE).reference);
        return *std::get<Surface>(source).data_;
    }
    template<typename Facade>
    OwnerRef StartupArenaOwner::source_owner(const std::variant<typed::ResourceId, Facade>& source, storage::StartupKind kind) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return resource_owner(find(*id, kind));
        return std::get<Facade>(source).owner_ref();
    }
    void StartupArenaOwner::construct(size_t index, const StartupInit& input) {
        auto& resource = resources[index];
        auto* storage = slot(resource);
        if (const auto* instance = std::get_if<InstanceCreateInfo>(&input)) {
            construct_instance(storage, *instance);
            resource.reference = &instance_at(storage);
        } else if (const auto* physical = std::get_if<PhysicalDeviceInit>(&input)) {
            auto& parent = resolve(physical->instance);
            auto candidate = operation::PhysicalDeviceCandidate{};
            if (physical->index) {
                const auto candidates = operation::enumerate_physical_devices(parent);
                candidate = operation::inspect_physical_device(parent, candidates.at(*physical->index));
            } else
                candidate = operation::select_physical_device(parent);
            construct_physical_device(storage, parent, candidate);
            resource.reference = &physical_device_at(storage);
        } else if (const auto* surface = std::get_if<SurfaceInit>(&input)) {
            construct_surface(storage, resolve(surface->instance), *WindowOwnerAccess::Ref(surface->window));
            resource.reference = &surface_at(storage);
        } else if (const auto* device = std::get_if<DeviceInit>(&input)) {
            construct_device(storage, resolve(device->physical), device->info);
            resource.reference = &device_at(storage);
        } else if (const auto* graphics = std::get_if<GraphicsDeviceInit>(&input)) {
            auto& physical = resolve(graphics->physical);
            auto info = operation::graphics_device_info(physical, resolve(graphics->surface), graphics->validation, graphics->present_fences);
            construct_device(storage, physical, std::move(info));
            resource.reference = &device_at(storage);
        }
    }
    void StartupArenaOwner::clear() noexcept {
        for (auto i = constructed.rbegin(); i != constructed.rend(); ++i) {
            const auto& resource = resources[*i];
            switch (resource.placement.kind) {
                case storage::StartupKind::INSTANCE:
                    destroy_instance(slot(resource));
                    break;
                case storage::StartupKind::PHYSICAL_DEVICE:
                    destroy_physical_device(slot(resource));
                    break;
                case storage::StartupKind::SURFACE:
                    destroy_surface(slot(resource));
                    break;
                case storage::StartupKind::DEVICE:
                    destroy_device(slot(resource));
                    break;
            }
        }
        constructed.clear();
        if (claimed) {
            memory->release();
            claimed = false;
        }
    }
    void StartupArenaOwner::publish(const std::shared_ptr<StartupArenaOwner>& owner, std::span<const StartupInit> inputs) {
        node.publish(owner, this, VisitDependencies);
        for (const auto i : constructed)
            std::visit([&](auto* value) { value->owner_node = &node; }, resources[i].reference);
        for (const auto i : constructed) {
            if (const auto* physical = std::get_if<PhysicalDeviceInit>(&inputs[i])) {
                auto& instance = resolve(physical->instance);
                set_physical_device_instance_owner(physical_device_at(slot(resources[i])),
                                                   source_owner(physical->instance, storage::StartupKind::INSTANCE).alias(&instance));
            } else if (const auto* surface = std::get_if<SurfaceInit>(&inputs[i])) {
                auto& instance = resolve(surface->instance);
                set_surface_owners(surface_at(slot(resources[i])),
                                   source_owner(surface->instance, storage::StartupKind::INSTANCE).alias(&instance), surface->window);
            } else if (const auto* device = std::get_if<DeviceInit>(&inputs[i])) {
                auto& physical = resolve(device->physical);
                set_device_physical_owner(device_at(slot(resources[i])),
                                          source_owner(device->physical, storage::StartupKind::PHYSICAL_DEVICE).alias(&physical));
            } else if (const auto* graphics = std::get_if<GraphicsDeviceInit>(&inputs[i])) {
                auto& physical = resolve(graphics->physical);
                set_device_physical_owner(device_at(slot(resources[i])),
                                          source_owner(graphics->physical, storage::StartupKind::PHYSICAL_DEVICE).alias(&physical));
            }
        }
    }
    void StartupArenaOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
        for (const auto& dependency : static_cast<const StartupArenaOwner*>(owner)->dependencies)
            visitor(context, dependency);
    }
} // namespace VSL_NAMESPACE::lifetime::detail

namespace VSL_NAMESPACE::lifetime {
    StartupArena::StartupArena(std::span<const storage::StartupPlacement> placements, std::span<const StartupInit> initializers,
                               std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment) {
        // native/error callbackで元入力が変わっても構築・公開を同じ所有値から完了する。
        const std::vector<StartupInit> inputs(initializers.begin(), initializers.end());
        _owner = std::make_shared<detail::StartupArenaOwner>(placements, inputs, std::move(memory), bytes, alignment);
        _owner->publish(_owner, inputs);
    }
    OwnerRef StartupArena::owner_ref() const {
        if (!_owner)
            throw std::logic_error("Startup arena has no owner");
        return _owner->node.ref(_owner);
    }
    size_t StartupArena::size() const {
        return _owner ? _owner->resources.size() : 0;
    }
    size_t StartupArena::slots() const {
        return _owner ? _owner->constructed.size() : 0;
    }
    VulkanAccessor StartupArena::instance(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Startup arena has no owner");
        const auto& resource = _owner->find(id, storage::StartupKind::INSTANCE);
        return StartupAccess::FromOwned(_owner->resource_owner(resource).alias(std::get<impl::VulkanImplAccessor*>(resource.reference)));
    }
    PhysicalDevice StartupArena::physical_device(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Startup arena has no owner");
        const auto& resource = _owner->find(id, storage::StartupKind::PHYSICAL_DEVICE);
        return StartupAccess::FromOwned(_owner->resource_owner(resource).alias(std::get<impl::PhysicalDeviceImpl*>(resource.reference)));
    }
    Surface StartupArena::surface(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Startup arena has no owner");
        const auto& resource = _owner->find(id, storage::StartupKind::SURFACE);
        return Surface(_owner->resource_owner(resource).alias(std::get<impl::SurfaceImpl*>(resource.reference)).share());
    }
    LogicalDeviceAccessor StartupArena::device(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Startup arena has no owner");
        const auto& resource = _owner->find(id, storage::StartupKind::DEVICE);
        return {_owner->resource_owner(resource).alias(std::get<impl::LogicalDeviceImpl*>(resource.reference)).share()};
    }
} // namespace VSL_NAMESPACE::lifetime
