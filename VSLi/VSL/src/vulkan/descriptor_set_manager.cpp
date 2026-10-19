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
        : identity_(std::make_shared<_impl::GraphicResourceManagerIdentity>()) { identity_->manager = this; }
    ManagerInterface::~ManagerInterface() = default;
    ManagerInterface::ManagerInterface(const ManagerInterface& other) : ManagerInterface() { _data = other._data; }
    ManagerInterface& ManagerInterface::operator=(const ManagerInterface& other) {
        if (this != &other) {
            auto identity = std::make_shared<_impl::GraphicResourceManagerIdentity>();
            identity->manager = this;
            _data = other._data;
            identity_ = std::move(identity);
        }
        return *this;
    }
    ManagerInterface::ManagerInterface(ManagerInterface&& other) noexcept
        : _data(std::move(other._data)), identity_(std::move(other.identity_)) {
        if (identity_)
            identity_->manager = this;
    }
    ManagerInterface& ManagerInterface::operator=(ManagerInterface&& other) noexcept {
        if (this != &other) {
            _data = std::move(other._data);
            identity_ = std::move(other.identity_);
            if (identity_)
                identity_->manager = this;
        }
        return *this;
    }
} // namespace VSL_NAMESPACE::graphic_resource

namespace {
    void release_descriptor_pool(const std::shared_ptr<VSL_NAMESPACE::_impl::GraphicResourcePool_impl>& data) {
        if (data->descriptorPool) {
            data->device->device.destroyDescriptorPool(data->descriptorPool);
            data->descriptorPool = nullptr;
        }
        for (auto& entry : data->resources)
            if (auto resource = entry.lock()) {
                resource->descriptorSet = nullptr;
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
        for (const auto& bp : layout.getBindingPoints()) {
            auto& count = size[bp.bindingType];
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
    pool._data = std::make_shared<_impl::GraphicResourcePool_impl>();
    pool._data->manager = identity_;
    pool._data->device = _data->device;

    std::vector<vk::DescriptorPoolSize> poolSizes;
    // size_t size;
    for (const auto& type : {Type::Sampler,
                             Type::CombinedImageSampler,
                             Type::SampledImage,
                             Type::StorageImage,
                             Type::UniformTexelBuffer,
                             Type::StorageTexelBuffer,
                             Type::UniformBuffer,
                             Type::StorageBuffer,
                             Type::UniformBufferDynamic,
                             Type::StorageBufferDynamic,
                             Type::InputAttachment,
                             Type::InlineUniformBlock,
                             Type::AccelerationStructure,
                             Type::AccelerationStructureNv,
                             Type::SampleWeightImageQcom,
                             Type::BlockMatchImageQcom,
                             Type::MutableValve,
                             Type::PartitionedAccelerationStructureNv}) {
        vk::DescriptorPoolSize poolSize;
        poolSize.type = (vk::DescriptorType)type;
        if (count.contains(type))
            poolSize.descriptorCount = static_cast<uint32_t>(count[type]);
        else
            poolSize.descriptorCount = other;
        if (poolSize.descriptorCount != 0) {
            poolSizes.push_back(poolSize);
            // size += poolSize.descriptorCount;
        }
    }
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_POOL_SIZE);
    pool._data->descriptorPool = _data->device->device.createDescriptorPool(poolInfo);

    _data->pools.push_back(pool._data);
    return pool;
}

VSL_NAMESPACE::_impl::GraphicResourceManager_impl::~GraphicResourceManager_impl() {
    for (auto& pool : pools) {
        pool.reset();
    }
}

VSL_NAMESPACE::graphic_resource::ExactAllocateManager::ExactAllocateManager(LogicalDeviceAccessor device) {
    _data = std::make_shared<_impl::GraphicResourceManager_impl>();
    _data->device = device._data;
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
    if (!pool._data)
        return;
    const auto data = pool._data;
    if (std::erase(_data->pools, data) != 0)
        release_descriptor_pool(data);
}

VSL_NAMESPACE::graphic_resource::OverAllocateManager::OverAllocateManager(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                                          std::map<Type, size_t> count, size_t other)
    : size(count), other(other) {
    _data = std::make_shared<_impl::GraphicResourceManager_impl>();
    _data->device = device._data;
    pools.push_back(make(count, other));
}

VSL_NAMESPACE::graphic_resource::OverAllocateManager::OverAllocateManager(VSL_NAMESPACE::LogicalDeviceAccessor device) {
    _data = std::make_shared<_impl::GraphicResourceManager_impl>();
    _data->device = device._data;
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
    if (!pool._data)
        return;
    const auto data = pool._data;
    if (std::erase(_data->pools, data) == 0)
        return;
    std::erase_if(pools, [&](const auto& owned) { return owned._data == data; });
    release_descriptor_pool(data);
}
