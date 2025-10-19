//
// Created by morimoto_hibiki on 2025/06/09.
//
#ifdef _MSC_VER
#include "pch.h"
#endif

#include "../define.h"

#include "_pimpls.h"
#include "../exceptions.h"

#include "descriptor.hpp"

#include <utility>
#include <ranges>

vsl::graphic_resource::BindingLayout::BindingLayout(vsl::LogicalDeviceAccessor device,
                                                    vsl::graphic_resource::Type bindingType,
                                                    vsl::ShaderFlag shaderType)
        : BindingLayout(device, -1, bindingType, shaderType, 1) {}

vsl::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device, size_t binding,
                                                    Type bindingType, ShaderFlag shaderType, size_t amount) {
    _data = std::make_shared<_impl::ResourceBindingLayout_impl>();

    _data->layoutBindings.resize(1);
    _data->layoutBindings[0].binding = binding;
    _data->layoutBindings[0].descriptorType = (vk::DescriptorType) bindingType;
    _data->layoutBindings[0].stageFlags = (vk::ShaderStageFlagBits) shaderType;
    _data->layoutBindings[0].descriptorCount = amount;

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.bindingCount = 1;
    layoutCreateInfo.pBindings = _data->layoutBindings.data();

    _data->layout = device._data->device.createDescriptorSetLayout(layoutCreateInfo);
}

vsl::graphic_resource::BindingLayout::BindingLayout(vsl::LogicalDeviceAccessor device,
                                                    const std::vector<BindingPoint> &points) {
    _data = std::make_shared<_impl::ResourceBindingLayout_impl>();
    _data->device = device._data;

    _data->layoutBindings.resize(points.size());
    int i = 0;
    for (auto point: points) {
        _data->layoutBindings[i].binding = point.binding;
        _data->layoutBindings[i].descriptorType = (vk::DescriptorType) point.bindingType;
        _data->layoutBindings[i].stageFlags = (vk::ShaderStageFlagBits) point.shaderType;
        _data->layoutBindings[i].descriptorCount = point.amount;
        i++;
    }

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.bindingCount = _data->layoutBindings.size();
    layoutCreateInfo.pBindings = _data->layoutBindings.data();

    _data->layout = _data->device->device.createDescriptorSetLayout(layoutCreateInfo);
}

vsl::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device,
                                                    std::initializer_list<BindingPoint> points)
        : BindingLayout(device, std::vector<BindingPoint>(points.begin(), points.end())) {}

std::vector <vsl::graphic_resource::BindingPoint> vsl::graphic_resource::BindingLayout::getBindingPoints() const {
    std::vector <BindingPoint> result;

    result.resize(this->_data->layoutBindings.size());
    for (size_t i = 0; i < result.size(); i++) {
        auto &binding = this->_data->layoutBindings[i];
        result[i].binding = binding.binding;
        result[i].bindingType = (Type) binding.descriptorType;
        result[i].shaderType = (ShaderFlag)((uint32_t) binding.stageFlags);
        result[i].amount = binding.descriptorCount;
    }
    return result;
}

vsl::_impl::ResourceBindingLayout_impl::~ResourceBindingLayout_impl() {
    device->device.destroyDescriptorSetLayout(layout);
}

std::tuple <vsl::graphic_resource::Pool, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::ManagerInterface::make(std::vector <BindingLayout> layouts, std::map <Type, size_t> size,
                                              size_t other) {
    for (const auto &layout: layouts)
        for (const auto &bp: layout.getBindingPoints()) {
            if (not size.contains(bp.bindingType))
                size[bp.bindingType] = 0;
            size[bp.bindingType]++;
        }
    auto pool = this->make(size, other);
    const auto &[err, resources] = pool.bind(layouts);
    if (err) {
        // TODO: throw exception
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

vsl::graphic_resource::ManagerInterface *vsl::graphic_resource::Pool::getManager() {
    return (vsl::graphic_resource::ManagerInterface *) this->_data->manager;
}

std::tuple<bool, vsl::graphic_resource::Resource>
vsl::graphic_resource::Pool::bind(vsl::graphic_resource::BindingLayout layout) {
    auto result = std::make_shared<_impl::GraphicResource_impl>();
    result->pool = _data;
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout._data->layout;

    if (((ManagerInterface *) _data->manager)->_data->device->device
                .allocateDescriptorSets(&allocInfo, &result->descriptorSet) != vk::Result::eSuccess) {
        return {false, {}};
    }
    _data->resources.push_back(result);
    return {true, {result}};
}

std::tuple<bool, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::Pool::bind(std::vector <vsl::graphic_resource::BindingLayout> layouts) {
    std::tuple<bool, std::vector<vsl::graphic_resource::Resource>> result{true, {}};
    auto &[check, resources] = result;
    resources.resize(layouts.size());

    for (size_t i = 0; i < resources.size(); i++) {
        auto &r = resources[i];
        r._data = std::make_shared<_impl::GraphicResource_impl>();
        r._data->pool = this->_data;
        r._data->layout = layouts[i]._data;
    }

    auto vkLayouts = layouts
                     | std::views::transform([](const BindingLayout &layout) { return layout._data->layout; })
                     | std::ranges::to<std::vector>();
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = vkLayouts.size();
    allocInfo.pSetLayouts = vkLayouts.data();

    std::vector <vk::DescriptorSet> descriptorSets(vkLayouts.size());
    if (((ManagerInterface *) _data->manager)->_data->device->device.
            allocateDescriptorSets(&allocInfo, descriptorSets.data()) != vk::Result::eSuccess)
        return {false, {}};
    for (size_t i = 0; i < descriptorSets.size(); i++) {
        resources[i]._data->descriptorSet = descriptorSets[i];
        _data->resources.push_back(resources[i]._data);
    }
    return result;
}

vsl::_impl::GraphicResourcePool_impl::~GraphicResourcePool_impl() {
    ((vsl::graphic_resource::ManagerInterface *) manager)->destroy(
            GraphicResourcePool{std::shared_ptr<GraphicResourcePool_impl>(this, [](auto d) {})});
}

vsl::graphic_resource::Pool vsl::graphic_resource::Resource::getPool() {
    return {_data->pool};
}

void vsl::graphic_resource::Resource::update(vsl::BufferAccessor *buffer, size_t binding, std::optional <Type> type,
                                             size_t offset, std::optional <size_t> size) {
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = buffer->_data->buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = size ? size.value() : buffer->size();

    vk::WriteDescriptorSet descriptorWrite;
    descriptorWrite.dstSet = this->_data->descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    if (type)
        descriptorWrite.descriptorType = (vk::DescriptorType) type.value();
    else {
        if (_data->layout.expired())
            throw vsl::exceptions::RuntimeException("LayoutNotFound", "Layout not found!",
                                                    std::source_location::current());
        else {
            auto layout = _data->layout.lock();
            for (const auto &lb: layout->layoutBindings)
                if (lb.binding == binding) {
                    descriptorWrite.descriptorType = lb.descriptorType;
                    break;
                }
        }
    }
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;
    descriptorWrite.pImageInfo = nullptr; // Optional
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    ((ManagerInterface *) _data->pool->manager)->_data->device->device.updateDescriptorSets({descriptorWrite}, {});
}

void
vsl::graphic_resource::Resource::update(vsl::Image *image, size_t binding, std::optional <Type> type, size_t offset,
                                        std::optional <size_t> size) {
    vk::DescriptorImageInfo imageInfo;
    imageInfo.imageView = image->_data->view;
    imageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet descriptorWrite;
    descriptorWrite.dstSet = this->_data->descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    if (type)
        descriptorWrite.descriptorType = (vk::DescriptorType) type.value();
    else {
        if (not _data->layout.expired()) {
            auto layout = _data->layout.lock();
            for (const auto &lb: layout->layoutBindings)
                if (lb.binding == binding) {
                    descriptorWrite.descriptorType = lb.descriptorType;
                    goto found_layout;
                }
        }
    }
    throw vsl::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
    found_layout:

    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = nullptr;
    descriptorWrite.pImageInfo = &imageInfo;
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    ((ManagerInterface *) _data->pool->manager)->_data->device->device.updateDescriptorSets({descriptorWrite}, {});
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