//
// Created by morimoto_hibiki on 2025/10/24.
//

#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/descriptor_set_manager.hpp>

std::tuple <vsl::graphic_resource::Pool, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::ManagerInterface::make(std::vector<BindingLayout> layouts,
                                              std::map<Type, size_t> size,
                                              size_t other) {
    for (const auto &layout: layouts)
        for (const auto &bp: layout.getBindingPoints()) {
            if (not size.contains(bp.bindingType))
                size[bp.bindingType] = 0;
            size[bp.bindingType]++;
        }
    auto pool = this->make(size, other);
    const auto &[err, resources] = pool.bind(layouts);
    if (not err) {
        // FIXME: message
        throw vsl::exceptions::RuntimeException("error: layout can't bind to pool.");
    }
    return {pool, resources};
}

vsl::graphic_resource::Pool
vsl::graphic_resource::ManagerInterface::make(std::map <Type, size_t> count, size_t other) {
    GraphicResourcePool pool;
    pool._data = std::make_shared<_impl::GraphicResourcePool_impl>();
    pool._data->manager = this;

    std::vector <vk::DescriptorPoolSize> poolSizes;
    // size_t size;
    for (const auto &type: {Type::Sampler,
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
        poolSize.type = (vk::DescriptorType) type;
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

vsl::_impl::GraphicResourceManager_impl::~GraphicResourceManager_impl() {
    for (const auto &pool: pools)
        device->device.destroy(pool->descriptorPool);
}


vsl::graphic_resource::ExactAllocateManager::ExactAllocateManager(LogicalDeviceAccessor device) {
    _data = std::make_shared<_impl::GraphicResourceManager_impl>();
    _data->device = device._data;
}

std::tuple <std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::ExactAllocateManager::allocate(std::vector <BindingLayout> layouts) {
    auto [pool, resources] = make(layouts);
    return {std::vector(resources.size(), pool), resources};
}


void vsl::graphic_resource::ExactAllocateManager::destroy(const vsl::GraphicResourcePool &pool) {
    _data->pools.erase(std::remove(_data->pools.begin(), _data->pools.end(), pool._data), _data->pools.end());
    _data->device->device.destroy(pool._data->descriptorPool);
}

vsl::graphic_resource::OverAllocateManager::OverAllocateManager(vsl::LogicalDeviceAccessor device,
                                                                std::map <Type, size_t> count, size_t other)
        : size(count), other(other) {
    _data = std::make_shared<_impl::GraphicResourceManager_impl>();
    _data->device = device._data;
    pools.push_back(make(count, other));
}


vsl::graphic_resource::OverAllocateManager::OverAllocateManager(vsl::LogicalDeviceAccessor device) {
    _data = std::make_shared<_impl::GraphicResourceManager_impl>();
    _data->device = device._data;
}


std::tuple <std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::OverAllocateManager::allocate(std::vector <BindingLayout> layouts) {
    std::tuple <std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>> result;
    auto &[use_pools, resources] = result;
    for (auto &layout: layouts) {
        for (auto &pool: pools) {
            const auto &[err, resource] = pool.bind(layout);
            if (err)
                continue;
            pools.push_back(pool);
            resources.push_back(resource);
            goto found_pool;
        }
        {
            const auto &[pool, resource] = make({layout}, size, other);
            pools.push_back(pool);
            resources.push_back(resource[0]);
        }
        found_pool:
        continue;
    }
    return result;
}

void vsl::graphic_resource::OverAllocateManager::destroy(const vsl::graphic_resource::Pool &pool) {

}