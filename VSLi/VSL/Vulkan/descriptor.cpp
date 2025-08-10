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

vsl::ResourceBindingLayout::ResourceBindingLayout(LogicalDeviceAccessor device, ResourceType bindingType,
                                                  ShaderType shaderType)
        : ResourceBindingLayout(std::move(device), (size_t) -1, bindingType, shaderType, 1) {}

vsl::ResourceBindingLayout::ResourceBindingLayout(const LogicalDeviceAccessor &device, size_t binding,
                                                  ResourceType bindingType, ShaderType shaderType, size_t amount) {
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

vsl::ResourceBindingLayout::ResourceBindingLayout(LogicalDeviceAccessor device,
                                                  std::initializer_list<ResourceBindingPoint> points) {
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

std::vector<vsl::ResourceBindingPoint> vsl::ResourceBindingLayout::getBindingPoints() const {
    std::vector<ResourceBindingPoint> result;

    result.resize(this->_data->layoutBindings.size());
    for (size_t i = 0; i < result.size(); i++) {
        auto &binding = this->_data->layoutBindings[i];
        result[i].binding = binding.binding;
        result[i].bindingType = (ResourceType) binding.descriptorType;
        result[i].shaderType = (ShaderType) ((uint32_t) binding.stageFlags);
        result[i].amount = binding.descriptorCount;
    }
    return result;
}

vsl::_impl::ResourceBindingLayout_impl::~ResourceBindingLayout_impl() {
    device->device.destroyDescriptorSetLayout(layout);
}

vsl::GraphicResourceManager::GraphicResourceManager(LogicalDeviceAccessor device) {
    _data = std::make_shared<_impl::GraphicResourceManager_impl>();
    _data->device = device._data;
}

vsl::GraphicResourceManager::GraphicResourceManager(std::shared_ptr<_impl::GraphicResourceManager_impl> impl) : _data(
        impl) {}

vsl::GraphicResourcePool vsl::GraphicResourceManager::make(std::map<ResourceType, size_t> count, size_t other) {
    GraphicResourcePool pool;
    pool._data = std::make_shared<_impl::GraphicResourcePool_impl>();
    pool._data->manager = _data;

    std::vector<vk::DescriptorPoolSize> poolSizes;
    // size_t size;
    for (const auto &type: {ResourceType::Sampler,
                            ResourceType::CombinedImageSampler,
                            ResourceType::SampledImage,
                            ResourceType::StorageImage,
                            ResourceType::UniformTexelBuffer,
                            ResourceType::StorageTexelBuffer,
                            ResourceType::UniformBuffer,
                            ResourceType::StorageBuffer,
                            ResourceType::UniformBufferDynamic,
                            ResourceType::StorageBufferDynamic,
                            ResourceType::InputAttachment,
                            ResourceType::InlineUniformBlock,
                            ResourceType::AccelerationStructure,
                            ResourceType::AccelerationStructureNv,
                            ResourceType::SampleWeightImageQcom,
                            ResourceType::BlockMatchImageQcom,
                            ResourceType::MutableValve,
                            ResourceType::PartitionedAccelerationStructureNv,
                            ResourceType::MaxEnum}) {
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

std::tuple<vsl::GraphicResourcePool, std::vector<vsl::GraphicResource>>
vsl::GraphicResourceManager::make(std::vector<ResourceBindingLayout> layouts) {
    std::map<ResourceType, size_t> size;
    for (const auto &layout: layouts)
        for (const auto &bp: layout.getBindingPoints()) {
            if (not size.contains(bp.bindingType))
                size[bp.bindingType] = 0;
            size[bp.bindingType]++;
        }
    auto pool = make(size);
    return {pool, pool.bind(layouts)};
}

void vsl::GraphicResourceManager::destroy(const vsl::GraphicResourcePool &pool) {
    _data->device->device.destroy(pool._data->descriptorPool);
    _data->pools.erase(std::remove(_data->pools.begin(), _data->pools.end(), pool._data), _data->pools.end());
}


vsl::_impl::GraphicResourceManager_impl::~GraphicResourceManager_impl() {
    for (const auto &pool: pools)
        device->device.destroy(pool->descriptorPool);
}

vsl::GraphicResourceManager vsl::GraphicResourcePool::getManager() {
    return {this->_data->manager};
}

vsl::GraphicResource vsl::GraphicResourcePool::bind(vsl::ResourceBindingLayout layout) {
    auto result = std::make_shared<_impl::GraphicResource_impl>();
    result->pool = _data;
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout._data->layout;
    result->descriptorSet = _data->manager->device->device.allocateDescriptorSets(allocInfo)[0];
    _data->resources.push_back(result);
    return {result};
}

std::vector<vsl::GraphicResource> vsl::GraphicResourcePool::bind(std::vector<ResourceBindingLayout> layouts) {
    std::vector<vsl::GraphicResource> result;
    result.resize(layouts.size());

    for (size_t i = 0; i < result.size(); i++) {
        auto &r = result[i];
        r._data = std::make_shared<_impl::GraphicResource_impl>();
        r._data->pool = this->_data;
        r._data->layout = layouts[i]._data;
    }

    auto vkLayouts = layouts
                     | std::views::transform([](const ResourceBindingLayout &layout) { return layout._data->layout; })
                     | std::ranges::to<std::vector>();
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = vkLayouts.size();
    allocInfo.pSetLayouts = vkLayouts.data();

    auto descriptorSets = _data->manager->device->device.allocateDescriptorSets(allocInfo);
    for (size_t i = 0; i < descriptorSets.size(); i++) {
        result[i]._data->descriptorSet = descriptorSets[i];
        _data->resources.push_back(result[i]._data);
    }
    return result;
}

vsl::_impl::GraphicResourcePool_impl::~GraphicResourcePool_impl() {
    GraphicResourceManager{manager}.destroy(
            GraphicResourcePool{std::shared_ptr<GraphicResourcePool_impl>(this, [](auto d) {})});
}

vsl::GraphicResourcePool vsl::GraphicResource::getPool() {
    return {_data->pool};
}

void vsl::GraphicResource::update(vsl::BufferAccessor *buffer, size_t binding, std::optional<ResourceType> type,
                                  size_t offset, std::optional<size_t> size) {
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

    _data->pool->manager->device->device.updateDescriptorSets({descriptorWrite}, {});
}

void vsl::GraphicResource::update(vsl::Image *image, size_t binding, std::optional<ResourceType> type, size_t offset,
                                  std::optional<size_t> size) {
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
                    goto GRAPHIC_RESOURCE_UPDATE_FINISH_SEARCH_LAYOUT;
                }
        }
    }
    throw vsl::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
    GRAPHIC_RESOURCE_UPDATE_FINISH_SEARCH_LAYOUT:

    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = nullptr;
    descriptorWrite.pImageInfo = &imageInfo;
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    _data->pool->manager->device->device.updateDescriptorSets({descriptorWrite}, {});
}
