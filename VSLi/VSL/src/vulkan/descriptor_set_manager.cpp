#include <VSL/define.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>
#include <VSL/vulkan/descriptor_set_manager.hpp>
#include <VSL/lifetime/descriptor_access.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <limits>
#include <algorithm>

namespace VSL_NAMESPACE::graphic_resource {
    ManagerInterface::ManagerInterface() : _identity(std::make_shared<impl::GraphicResourceManagerIdentity>()) {
        _identity->manager = this;
    }
    ManagerInterface::ManagerInterface(LogicalDeviceAccessor device) : ManagerInterface() {
        if (device.data_)
            lifetime::require_device_owner(device);
        _state = std::make_shared<impl::GraphicResourceManagerImpl>();
        _state->device = device.data_;
    }
    ManagerInterface::~ManagerInterface() {
        retire_identity();
    }
    ManagerInterface::ManagerInterface(const ManagerInterface& other) : ManagerInterface() {
        _state = other._state;
    }
    ManagerInterface::ManagerInterface(ManagerInterface&& other) noexcept
        : _state(std::move(other._state)), _identity(std::move(other._identity)) {
        if (_identity)
            _identity->manager = this;
    }
    ManagerInterface& ManagerInterface::operator=(const ManagerInterface& other) {
        if (this == &other)
            return *this;
        auto state = other._state;
        auto identity = std::make_shared<impl::GraphicResourceManagerIdentity>();
        _state.swap(state);
        _identity.swap(identity);
        _identity->manager = this;
        if (identity)
            identity->manager = nullptr;
        return *this;
    }
    ManagerInterface& ManagerInterface::operator=(ManagerInterface&& other) noexcept {
        if (this == &other)
            return *this;
        auto state = std::move(other._state);
        auto identity = std::move(other._identity);
        _state.swap(state);
        _identity.swap(identity);
        if (_identity)
            _identity->manager = this;
        if (identity)
            identity->manager = nullptr;
        return *this;
    }
    void ManagerInterface::swap_state(ManagerInterface& other) noexcept {
        _state.swap(other._state);
        _identity.swap(other._identity);
        if (_identity)
            _identity->manager = this;
        if (other._identity)
            other._identity->manager = &other;
    }
    void ManagerInterface::retire_identity() noexcept {
        if (_identity)
            _identity->manager = nullptr;
    }
    LogicalDeviceAccessor ManagerInterface::get_device() const {
        const auto state = _state;
        if (!state)
            throw std::logic_error("Descriptor manager is empty");
        return LogicalDeviceAccessor{state->device};
    }
} // namespace VSL_NAMESPACE::graphic_resource

namespace {
    using namespace VSL_NAMESPACE;
    using namespace VSL_NAMESPACE::graphic_resource;
    using lifetime::detail::DescriptorAccess;
    using lifetime::detail::DescriptorManagerAccess;
    using lifetime::detail::DescriptorManagerContext;
    using PoolOwner = lifetime::OwnedRef<impl::GraphicResourcePoolImpl>;
    struct PoolRetirement {
        impl::GraphicResourcePoolImpl& pool;
        explicit PoolRetirement(impl::GraphicResourcePoolImpl& pool);
        ~PoolRetirement() { pool.retiring = false; }
    };
    PoolRetirement::PoolRetirement(impl::GraphicResourcePoolImpl& pool) : pool(pool) {
        if (pool.retiring)
            throw std::logic_error("Descriptor pool is being destroyed");
        pool.retiring = true;
    }
    void require_pool_idle(const PoolOwner& data) {
        std::vector<std::shared_ptr<impl::GraphicResourceImpl>> resources;
        for (const auto& entry : data->resources)
            if (auto resource = entry.lock())
                resources.push_back(std::move(resource));
        for (const auto& resource : resources) {
            if (resource->recording || (resource->owner_context && resource->owner_context->node.updating()))
                throw std::logic_error("Descriptor resource is being recorded");
            resource->use.require_idle();
        }
    }
    template<typename Detach>
    void release_descriptor_pool(const PoolOwner& data, Detach&& detach) {
        std::vector<std::shared_ptr<impl::GraphicResourceImpl>> resources;
        resources.reserve(data->resources.size());
        for (const auto& entry : data->resources)
            if (auto resource = entry.lock())
                resources.push_back(std::move(resource));
        std::vector<decltype(impl::GraphicResourceImpl::bindings)> previous(resources.size());
        if (!detach())
            return;
        operation::destroy_descriptor_pool(*data);
        for (size_t i = 0; i < resources.size(); ++i) {
            resources[i]->descriptor_set = nullptr;
            resources[i]->bindings.swap(previous[i]);
        }
        data->resources.clear();
    }
    OverAllocateManager* current_over_allocator(const DescriptorManagerContext& context) {
        if (!context.identity || !context.identity->manager)
            return nullptr;
        auto* manager = dynamic_cast<OverAllocateManager*>(context.identity->manager);
        return manager && DescriptorManagerAccess::Data(*manager) == context.state ? manager : nullptr;
    }
    Pool make_pool(const DescriptorManagerContext& context, std::map<Type, size_t> count, size_t other) {
        constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
        if (other > maximum)
            throw exceptions::RuntimeException("DescriptorPool", "Fallback descriptor count exceeds uint32_t");
        for (const auto& [type, amount] : count)
            if (amount > maximum)
                throw exceptions::RuntimeException("DescriptorPool", "Descriptor count exceeds uint32_t");
        DescriptorPoolCreateInfo info;
        for (const auto type : {Type::SAMPLER, Type::COMBINED_IMAGE_SAMPLER, Type::SAMPLED_IMAGE, Type::STORAGE_IMAGE,
                                Type::UNIFORM_TEXEL_BUFFER, Type::STORAGE_TEXEL_BUFFER, Type::UNIFORM_BUFFER, Type::STORAGE_BUFFER,
                                Type::UNIFORM_BUFFER_DYNAMIC, Type::STORAGE_BUFFER_DYNAMIC, Type::INPUT_ATTACHMENT,
                                Type::INLINE_UNIFORM_BLOCK, Type::ACCELERATION_STRUCTURE, Type::ACCELERATION_STRUCTURE_NV,
                                Type::SAMPLE_WEIGHT_IMAGE_QCOM, Type::BLOCK_MATCH_IMAGE_QCOM, Type::MUTABLE_VALVE,
                                Type::PARTITIONED_ACCELERATION_STRUCTURE_NV}) {
            const auto capacity = count.contains(type) ? count.at(type) : other;
            if (capacity)
                info.descriptors.emplace(type, capacity);
        }
        if (!context.state)
            throw std::logic_error("Descriptor manager is empty");
        auto pool = DescriptorAccess::FromShared(lifetime::make_heap_descriptor_pool(LogicalDeviceAccessor{context.state->device}, info));
        DescriptorAccess::Owned(pool)->manager = context.identity;
        context.state->pools.push_back(lifetime::detail::OwnerAccess::Shared(DescriptorAccess::Owned(pool)));
        return pool;
    }
    std::tuple<Pool, std::vector<Resource>> make_resources(const DescriptorManagerContext& context,
                                                           std::vector<BindingLayout> layouts, std::map<Type, size_t> count = {}, size_t other = 0) {
        for (const auto& layout : layouts)
            for (const auto& point : layout.get_binding_points()) {
                auto& value = count[point.binding_type];
                constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
                if (value > maximum || point.amount > maximum - value)
                    throw exceptions::RuntimeException("DescriptorPool", "Descriptor count exceeds uint32_t");
                value += point.amount;
            }
        auto pool = make_pool(context, std::move(count), other);
        auto [success, resources] = pool.bind(std::move(layouts));
        if (!success)
            throw exceptions::RuntimeException("error: layout can't bind to pool.");
        return {std::move(pool), std::move(resources)};
    }
} // namespace

namespace VSL_NAMESPACE::graphic_resource {
    Pool ManagerInterface::make(std::map<Type, size_t> count, size_t other) {
        const auto context = DescriptorManagerAccess::Snapshot(*this);
        return make_pool(context, std::move(count), other);
    }
    std::tuple<Pool, std::vector<Resource>> ManagerInterface::make(std::vector<BindingLayout> layouts, std::map<Type, size_t> count, size_t other) {
        const auto context = DescriptorManagerAccess::Snapshot(*this);
        return make_resources(context, std::move(layouts), std::move(count), other);
    }
    ExactAllocateManager::ExactAllocateManager(LogicalDeviceAccessor device) : ManagerInterface(std::move(device)) {}
    std::tuple<std::vector<Pool>, std::vector<Resource>> ExactAllocateManager::allocate(std::vector<BindingLayout> layouts) {
        const auto context = DescriptorManagerAccess::Snapshot(*this);
        std::vector<Pool> pools;
        std::vector<Resource> resources;
        pools.reserve(layouts.size());
        resources.reserve(layouts.size());
        for (auto& layout : layouts) {
            auto [pool, allocated] = make_resources(context, std::vector{layout});
            pools.push_back(std::move(pool));
            resources.push_back(std::move(allocated.front()));
        }
        return {std::move(pools), std::move(resources)};
    }
    void ManagerInterface::destroy_pool(const Pool& pool) {
        const auto data = DescriptorAccess::Owned(pool);
        const auto context = DescriptorManagerAccess::Snapshot(*this);
        if (!data || !context.state || !std::ranges::contains(context.state->pools, lifetime::detail::OwnerAccess::Shared(data)))
            return;
        const PoolRetirement retirement(*data);
        require_pool_idle(data);
        release_descriptor_pool(data, [&] { return std::erase(context.state->pools, lifetime::detail::OwnerAccess::Shared(data)) != 0; });
    }
    void ExactAllocateManager::destroy(const Pool& pool) {
        destroy_pool(pool);
    }
    OverAllocateManager::OverAllocateManager(LogicalDeviceAccessor device, std::map<Type, size_t> count, size_t other)
        : ManagerInterface(std::move(device)), size(std::move(count)), other(other) {
        pools.push_back(make(size, other));
    }
    OverAllocateManager::OverAllocateManager(LogicalDeviceAccessor device) : ManagerInterface(std::move(device)) {}
    OverAllocateManager::OverAllocateManager(const OverAllocateManager& source)
        : ManagerInterface(source), size(source.size), other(source.other), pools(source.pools) {}
    OverAllocateManager::OverAllocateManager(OverAllocateManager&& source) noexcept
        : ManagerInterface(std::move(source)), size(std::move(source.size)), other(std::exchange(source.other, 0)), pools(std::move(source.pools)) {}
    OverAllocateManager& OverAllocateManager::operator=(const OverAllocateManager& source) {
        if (this == &source)
            return *this;
        OverAllocateManager replacement(source);
        swap(replacement);
        return *this;
    }
    OverAllocateManager& OverAllocateManager::operator=(OverAllocateManager&& source) noexcept {
        if (this == &source)
            return *this;
        OverAllocateManager replacement(std::move(source));
        swap(replacement);
        return *this;
    }
    OverAllocateManager::~OverAllocateManager() {
        retire_identity();
    }
    void OverAllocateManager::swap(OverAllocateManager& other) noexcept {
        size.swap(other.size);
        std::swap(this->other, other.other);
        pools.swap(other.pools);
        swap_state(other);
    }
    std::tuple<std::vector<Pool>, std::vector<Resource>> OverAllocateManager::allocate(std::vector<BindingLayout> layouts) {
        const auto context = DescriptorManagerAccess::Snapshot(*this);
        const auto count = size;
        const auto fallback = other;
        auto cached = pools;
        std::vector<Pool> selected;
        std::vector<Resource> resources;
        selected.reserve(layouts.size());
        resources.reserve(layouts.size());
        for (const auto& layout : layouts) {
            bool found = false;
            for (auto& pool : cached) {
                auto [success, allocated] = pool.bind(layout);
                if (!success)
                    continue;
                selected.push_back(pool);
                resources.push_back(std::move(allocated));
                found = true;
                break;
            }
            if (!found) {
                auto [pool, allocated] = make_resources(context, std::vector{layout}, count, fallback);
                cached.push_back(pool);
                if (auto* current = current_over_allocator(context))
                    current->pools.push_back(pool);
                selected.push_back(std::move(pool));
                resources.push_back(std::move(allocated.front()));
            }
        }
        return {std::move(selected), std::move(resources)};
    }
    void OverAllocateManager::destroy(const Pool& pool) {
        const auto data = DescriptorAccess::Owned(pool);
        const auto context = DescriptorManagerAccess::Snapshot(*this);
        if (!data || !context.state || !std::ranges::contains(context.state->pools, lifetime::detail::OwnerAccess::Shared(data)))
            return;
        const PoolRetirement retirement(*data);
        require_pool_idle(data);
        release_descriptor_pool(data, [&] {
            auto* current = current_over_allocator(context);
            auto next = current ? current->pools : std::vector<Pool>{};
            std::erase_if(next, [&](const auto& value) { return DescriptorAccess::Owned(value).get() == data.get(); });
            if (!std::erase(context.state->pools, lifetime::detail::OwnerAccess::Shared(data)))
                return false;
            if (current)
                current->pools.swap(next);
            return true;
        });
    }
} // namespace VSL_NAMESPACE::graphic_resource

VSL_NAMESPACE::impl::GraphicResourceManagerImpl::~GraphicResourceManagerImpl() {
    for (auto& pool : pools)
        pool.reset();
}
