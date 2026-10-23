//
// Created by morimoto_hibiki on 2025/10/24.
//

#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>
#include <limits>

#include <VSL/vulkan/descriptor_set_manager.hpp>

namespace VSL_NAMESPACE::graphic_resource {
    ManagerInterface::ManagerInterface()
        : _identity(std::make_shared<impl::GraphicResourceManagerIdentity>()) {
        _identity->manager = this;
    }
    ManagerInterface::~ManagerInterface() = default;
    ManagerInterface::ManagerInterface(const ManagerInterface& other) : ManagerInterface() {
        data_ = other.data_;
    }
    ManagerInterface& ManagerInterface::operator=(const ManagerInterface& other) {
        if (this != &other) {
            auto identity = std::make_shared<impl::GraphicResourceManagerIdentity>();
            identity->manager = this;
            data_ = other.data_;
            _identity = std::move(identity);
        }
        return *this;
    }
    ManagerInterface::ManagerInterface(ManagerInterface&& other) noexcept
        : data_(std::move(other.data_)), _identity(std::move(other._identity)) {
        if (_identity)
            _identity->manager = this;
    }
    ManagerInterface& ManagerInterface::operator=(ManagerInterface&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            _identity = std::move(other._identity);
            if (_identity)
                _identity->manager = this;
        }
        return *this;
    }
} // namespace VSL_NAMESPACE::graphic_resource

namespace {
    void release_descriptor_pool(const std::shared_ptr<VSL_NAMESPACE::impl::GraphicResourcePoolImpl>& data) {
        if (data->descriptor_pool) {
            data->device->device.destroyDescriptorPool(data->descriptor_pool);
            data->descriptor_pool = nullptr;
        }
        for (auto& entry : data->resources)
            if (auto resource = entry.lock()) {
                resource->descriptor_set = nullptr;
                resource->bindings.clear();
            }
        data->resources.clear();
    }
} // namespace

std::tuple<VSL_NAMESPACE::graphic_resource::Pool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::ManagerInterface::make(std::vector<BindingLayout> layouts,
                                                        std::map<Type, size_t> size,
                                                        size_t other) {
    for (const auto& layout : layouts)
        for (const auto& bp : layout.get_binding_points()) {
            auto& count = size[bp.binding_type];
            constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
            if (count > maximum || bp.amount > maximum - count)
                throw VSL_NAMESPACE::exceptions::RuntimeException("DescriptorPool", "Descriptor count exceeds uint32_t");
            count += bp.amount;
        }
    auto pool = this->make(size, other);
    const auto& [err, resources] = pool.bind(layouts);
    if (not err) {
        // FIXME: message
        throw VSL_NAMESPACE::exceptions::RuntimeException("error: layout can't bind to pool.");
    }
    return {pool, resources};
}

VSL_NAMESPACE::graphic_resource::Pool
VSL_NAMESPACE::graphic_resource::ManagerInterface::make(std::map<Type, size_t> count, size_t other) {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    if (other > maximum)
        throw VSL_NAMESPACE::exceptions::RuntimeException("DescriptorPool", "Fallback descriptor count exceeds uint32_t");
    for (const auto& [type, amount] : count)
        if (amount > maximum)
            throw VSL_NAMESPACE::exceptions::RuntimeException("DescriptorPool", "Descriptor count exceeds uint32_t");

    GraphicResourcePool pool;
    pool.data_ = std::make_shared<impl::GraphicResourcePoolImpl>();
    pool.data_->manager = _identity;
    pool.data_->device = data_->device;

    std::vector<vk::DescriptorPoolSize> pool_sizes;
    // size_t size;
    for (const auto& type : {Type::SAMPLER,
                             Type::COMBINED_IMAGE_SAMPLER,
                             Type::SAMPLED_IMAGE,
                             Type::STORAGE_IMAGE,
                             Type::UNIFORM_TEXEL_BUFFER,
                             Type::STORAGE_TEXEL_BUFFER,
                             Type::UNIFORM_BUFFER,
                             Type::STORAGE_BUFFER,
                             Type::UNIFORM_BUFFER_DYNAMIC,
                             Type::STORAGE_BUFFER_DYNAMIC,
                             Type::INPUT_ATTACHMENT,
                             Type::INLINE_UNIFORM_BLOCK,
                             Type::ACCELERATION_STRUCTURE,
                             Type::ACCELERATION_STRUCTURE_NV,
                             Type::SAMPLE_WEIGHT_IMAGE_QCOM,
                             Type::BLOCK_MATCH_IMAGE_QCOM,
                             Type::MUTABLE_VALVE,
                             Type::PARTITIONED_ACCELERATION_STRUCTURE_NV}) {
        vk::DescriptorPoolSize pool_size;
        pool_size.type = (vk::DescriptorType)type;
        if (count.contains(type))
            pool_size.descriptorCount = static_cast<uint32_t>(count[type]);
        else
            pool_size.descriptorCount = other;
        if (pool_size.descriptorCount != 0) {
            pool_sizes.push_back(pool_size);
            // size += poolSize.descriptorCount;
        }
    }
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(MAX_POOL_SIZE);
    pool.data_->descriptor_pool = data_->device->device.createDescriptorPool(pool_info);

    data_->pools.push_back(pool.data_);
    return pool;
}

VSL_NAMESPACE::impl::GraphicResourceManagerImpl::~GraphicResourceManagerImpl() {
    for (auto& pool : pools) {
        pool.reset();
    }
}

VSL_NAMESPACE::graphic_resource::ExactAllocateManager::ExactAllocateManager(LogicalDeviceAccessor device) {
    data_ = std::make_shared<impl::GraphicResourceManagerImpl>();
    data_->device = device.data_;
}

std::tuple<std::vector<VSL_NAMESPACE::graphic_resource::Pool>, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::ExactAllocateManager::allocate(std::vector<BindingLayout> layouts) {
    std::vector<Pool> pools;
    std::vector<Resource> resources;
    pools.reserve(layouts.size());
    resources.reserve(layouts.size());
    for (auto& layout : layouts) {
        auto [pool, allocated] = make(std::vector{layout});
        pools.push_back(std::move(pool));
        resources.push_back(std::move(allocated.front()));
    }
    return {std::move(pools), std::move(resources)};
}

void VSL_NAMESPACE::graphic_resource::ExactAllocateManager::destroy(const VSL_NAMESPACE::GraphicResourcePool& pool) {
    if (!pool.data_)
        return;
    const auto data = pool.data_;
    if (std::erase(data_->pools, data) != 0)
        release_descriptor_pool(data);
}

VSL_NAMESPACE::graphic_resource::OverAllocateManager::OverAllocateManager(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                                          std::map<Type, size_t> count, size_t other)
    : size(count), other(other) {
    data_ = std::make_shared<impl::GraphicResourceManagerImpl>();
    data_->device = device.data_;
    pools.push_back(make(count, other));
}

VSL_NAMESPACE::graphic_resource::OverAllocateManager::OverAllocateManager(VSL_NAMESPACE::LogicalDeviceAccessor device) {
    data_ = std::make_shared<impl::GraphicResourceManagerImpl>();
    data_->device = device.data_;
}

std::tuple<std::vector<VSL_NAMESPACE::graphic_resource::Pool>, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::OverAllocateManager::allocate(std::vector<BindingLayout> layouts) {
    std::tuple<std::vector<VSL_NAMESPACE::graphic_resource::Pool>, std::vector<VSL_NAMESPACE::graphic_resource::Resource>> result;
    auto& [use_pools, resources] = result;
    for (auto& layout : layouts) {
        for (auto& pool : pools) {
            const auto& [err, resource] = pool.bind(layout);
            if (not err)
                continue;
            use_pools.push_back(pool);
            resources.push_back(resource);
            goto found_pool;
        }
        {
            const auto& [pool, resource] = make({layout}, size, other);
            pools.push_back(pool);
            use_pools.push_back(pool);
            resources.push_back(resource[0]);
        }
    found_pool:
        continue;
    }
    return result;
}

void VSL_NAMESPACE::graphic_resource::OverAllocateManager::destroy(const VSL_NAMESPACE::graphic_resource::Pool& pool) {
    if (!pool.data_)
        return;
    const auto data = pool.data_;
    if (std::erase(data_->pools, data) == 0)
        return;
    std::erase_if(pools, [&](const auto& owned) { return owned.data_ == data; });
    release_descriptor_pool(data);
}
