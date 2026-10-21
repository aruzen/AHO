//
// Created by morimoto_hibiki on 2025/06/09.
//
#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/descriptor.hpp>
#include <VSL/vulkan/descriptor_set_manager.hpp>

#include <utility>
#include <ranges>

namespace {
    void validate_pool(const std::shared_ptr<VSL_NAMESPACE::_impl::GraphicResourcePool_impl>& pool) {
        if (!pool || !pool->descriptorPool)
            throw std::logic_error("Descriptor pool has been destroyed");
        std::erase_if(pool->resources, [](const auto& resource) { return resource.expired(); });
    }
    void validate_layout(const std::shared_ptr<VSL_NAMESPACE::_impl::GraphicResourcePool_impl>& pool,
                         const VSL_NAMESPACE::graphic_resource::BindingLayout& layout) {
        if (!layout._data || layout._data->device != pool->device)
            throw std::invalid_argument("Descriptor layout device mismatch");
    }
    void validate_resource(const std::shared_ptr<VSL_NAMESPACE::_impl::GraphicResource_impl>& resource) {
        if (!resource || !resource->pool || !resource->pool->descriptorPool || !resource->descriptorSet)
            throw std::logic_error("Descriptor resource has been destroyed");
    }
} // namespace

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                              VSL_NAMESPACE::graphic_resource::Type bindingType,
                                                              VSL_NAMESPACE::ShaderFlag shaderType)
    : BindingLayout(device, 0, bindingType, shaderType, 1) {}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device, size_t binding,
                                                              Type bindingType, ShaderFlag shaderType, size_t amount) {
    _data = std::make_shared<_impl::ResourceBindingLayout_impl>();
    _data->device = device._data;

    _data->layoutBindings.resize(1);
    _data->layoutBindings[0].binding = binding == size_t(-1) || binding == UINT32_MAX ? 0 : binding;
    _data->layoutBindings[0].descriptorType = (vk::DescriptorType)bindingType;
    _data->layoutBindings[0].stageFlags = (vk::ShaderStageFlagBits)shaderType;
    _data->layoutBindings[0].descriptorCount = amount;

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.bindingCount = 1;
    layoutCreateInfo.pBindings = _data->layoutBindings.data();

    _data->layout = device._data->device.createDescriptorSetLayout(layoutCreateInfo);
}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                              const std::vector<BindingPoint>& points) {
    _data = std::make_shared<_impl::ResourceBindingLayout_impl>();
    _data->device = device._data;

    _data->layoutBindings.resize(points.size());
    for (size_t i = 0; i < points.size(); i++) {
        auto& point = points[i];
        _data->layoutBindings[i].binding = point.binding == (std::uint32_t)-1 ? (std::uint32_t)i : point.binding;
        _data->layoutBindings[i].descriptorType = (vk::DescriptorType)point.bindingType;
        _data->layoutBindings[i].stageFlags = (vk::ShaderStageFlagBits)point.shaderType;
        _data->layoutBindings[i].descriptorCount = point.amount;
    }

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.bindingCount = _data->layoutBindings.size();
    layoutCreateInfo.pBindings = _data->layoutBindings.data();

    _data->layout = _data->device->device.createDescriptorSetLayout(layoutCreateInfo);
}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device,
                                                              std::initializer_list<BindingPoint> points)
    : BindingLayout(device, std::vector<BindingPoint>(points.begin(), points.end())) {}

std::vector<VSL_NAMESPACE::graphic_resource::BindingPoint> VSL_NAMESPACE::graphic_resource::BindingLayout::getBindingPoints() const {
    std::vector<BindingPoint> result;

    result.resize(this->_data->layoutBindings.size());
    for (size_t i = 0; i < result.size(); i++) {
        auto& binding = this->_data->layoutBindings[i];
        result[i].binding = binding.binding == (std::uint32_t)-1 ? (std::uint32_t)i : binding.binding;
        result[i].bindingType = (Type)binding.descriptorType;
        result[i].shaderType = (ShaderFlag)((uint32_t)binding.stageFlags);
        result[i].amount = binding.descriptorCount;
    }
    return result;
}

VSL_NAMESPACE::_impl::ResourceBindingLayout_impl::~ResourceBindingLayout_impl() {
    device->device.destroyDescriptorSetLayout(layout);
}

VSL_NAMESPACE::graphic_resource::ManagerInterface* VSL_NAMESPACE::graphic_resource::Pool::getManager() {
    auto identity = _data ? _data->manager.lock() : nullptr;
    return identity ? identity->manager : nullptr;
}

std::tuple<bool, VSL_NAMESPACE::graphic_resource::Resource>
VSL_NAMESPACE::graphic_resource::Pool::bind(VSL_NAMESPACE::graphic_resource::BindingLayout layout) {
    validate_pool(_data);
    validate_layout(_data, layout);
    auto result = std::make_shared<_impl::GraphicResource_impl>();
    result->pool = _data;
    result->layout = layout._data;
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout._data->layout;

    if (_data->device->device.allocateDescriptorSets(&allocInfo, &result->descriptorSet) != vk::Result::eSuccess) {
        return {false, {}};
    }
    _data->resources.push_back(result);
    return {true, {result}};
}

std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::Pool::bind(size_t n, VSL_NAMESPACE::graphic_resource::BindingLayout layout) {
    validate_pool(_data);
    validate_layout(_data, layout);
    if (n == 0)
        return {true, {}};
    std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>> result{true, {}};
    auto& [check, resources] = result;
    resources.resize(n);

    auto vkLayouts = std::vector(n, layout._data->layout);

    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = vkLayouts.size();
    allocInfo.pSetLayouts = vkLayouts.data();

    std::vector<vk::DescriptorSet> descriptorSets;
    descriptorSets.resize(vkLayouts.size());
    if (_data->device->device.allocateDescriptorSets(&allocInfo, descriptorSets.data()) != vk::Result::eSuccess)
        return {false, {}};
    for (size_t i = 0; i < descriptorSets.size(); i++) {
        resources[i] = Resource{std::make_shared<_impl::GraphicResource_impl>()};
        resources[i]._data->pool = _data;
        resources[i]._data->layout = layout._data;
        resources[i]._data->descriptorSet = descriptorSets[i];
        _data->resources.push_back(resources[i]._data);
    }
    return result;
}

std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::Pool::bind(std::vector<VSL_NAMESPACE::graphic_resource::BindingLayout> layouts) {
    validate_pool(_data);
    for (const auto& layout : layouts)
        validate_layout(_data, layout);
    if (layouts.empty())
        return {true, {}};
    std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>> result{true, {}};
    auto& [check, resources] = result;
    resources.resize(layouts.size());

    for (size_t i = 0; i < resources.size(); i++) {
        auto& r = resources[i];
        r._data = std::make_shared<_impl::GraphicResource_impl>();
        r._data->pool = this->_data;
        r._data->layout = layouts[i]._data;
    }

    auto vkLayouts = layouts | std::views::transform([](const BindingLayout& layout) { return layout._data->layout; }) | std::ranges::to<std::vector>();
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = vkLayouts.size();
    allocInfo.pSetLayouts = vkLayouts.data();

    std::vector<vk::DescriptorSet> descriptorSets(vkLayouts.size());
    if (_data->device->device.allocateDescriptorSets(&allocInfo, descriptorSets.data()) != vk::Result::eSuccess)
        return {false, {}};
    for (size_t i = 0; i < descriptorSets.size(); i++) {
        resources[i]._data->descriptorSet = descriptorSets[i];
        _data->resources.push_back(resources[i]._data);
    }
    return result;
}

VSL_NAMESPACE::_impl::GraphicResourcePool_impl::~GraphicResourcePool_impl() {
    device->device.destroyDescriptorPool(descriptorPool);
}

VSL_NAMESPACE::graphic_resource::Pool VSL_NAMESPACE::graphic_resource::Resource::getPool() {
    return {_data->pool};
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::BufferAccessor* buffer, size_t binding, std::optional<Type> type,
                                                       size_t offset, std::optional<size_t> size) {
    validate_resource(_data);
    if (!buffer || !buffer->_data || buffer->_data->device != _data->pool->device)
        throw std::invalid_argument("Descriptor buffer device mismatch");
    const auto buffer_size = buffer->_data->requirementSize;
    if (offset >= buffer_size)
        throw VSL_NAMESPACE::exceptions::RuntimeException("BufferRange", "Buffer descriptor range starts outside the buffer");
    const vk::DeviceSize range = size ? *size : buffer_size - offset;
    if (range == 0 || (range != VK_WHOLE_SIZE && range > buffer_size - offset))
        throw VSL_NAMESPACE::exceptions::RuntimeException("BufferRange", "Buffer descriptor range is empty or exceeds the buffer");

    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = buffer->_data->buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    vk::WriteDescriptorSet descriptorWrite;
    descriptorWrite.dstSet = this->_data->descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    if (type)
        descriptorWrite.descriptorType = (vk::DescriptorType)type.value();
    else {
        const auto layout = _data->layout;
        if (!layout)
            throw VSL_NAMESPACE::exceptions::RuntimeException("LayoutNotFound", "Layout not found!",
                                                              std::source_location::current());
        const auto found = std::ranges::find(layout->layoutBindings, binding,
                                             &vk::DescriptorSetLayoutBinding::binding);
        if (found == layout->layoutBindings.end())
            throw VSL_NAMESPACE::exceptions::RuntimeException("BindingNotFound", "Descriptor binding not found!");
        descriptorWrite.descriptorType = found->descriptorType;
    }
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;
    descriptorWrite.pImageInfo = nullptr;       // Optional
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    auto owners = std::vector<std::shared_ptr<void>>{buffer->_data};
    _data->bindings[static_cast<std::uint32_t>(binding)] = std::move(owners);
    _data->pool->device->device.updateDescriptorSets({descriptorWrite}, {});
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::ImageAccessor image, size_t binding, std::optional<Type> type) {
    validate_resource(_data);
    if (!image._data || image._data->device != _data->pool->device)
        throw std::invalid_argument("Descriptor image device mismatch");
    vk::DescriptorImageInfo imageInfo;
    imageInfo.imageView = image._data->view;
    imageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet descriptorWrite;
    descriptorWrite.dstSet = this->_data->descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    if (type) {
        descriptorWrite.descriptorType = (vk::DescriptorType)type.value();
        goto found_type;
    } else {
        if (_data->layout) {
            auto layout = _data->layout;
            for (const auto& lb : layout->layoutBindings)
                if (lb.binding == binding) {
                    descriptorWrite.descriptorType = lb.descriptorType;
                    goto found_type;
                }
        }
    }
    throw VSL_NAMESPACE::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
found_type:

    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = nullptr;
    descriptorWrite.pImageInfo = &imageInfo;
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    _data->bindings[static_cast<std::uint32_t>(binding)] = {image._data};
    _data->pool->device->device.updateDescriptorSets({descriptorWrite}, {});
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::ImageAccessor image, SamplerAccessor sampler,
                                                       size_t binding, std::optional<Type> type) {
    validate_resource(_data);
    if (!image._data || image._data->device != _data->pool->device)
        throw std::invalid_argument("Descriptor image device mismatch");
    vk::DescriptorImageInfo imageInfo;
    imageInfo.imageView = image._data->view;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    if (!sampler._data || sampler._data->device != _data->pool->device)
        throw std::invalid_argument("Descriptor sampler device mismatch");
    imageInfo.sampler = sampler._data->sampler;

    vk::WriteDescriptorSet descriptorWrite;
    descriptorWrite.dstSet = this->_data->descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    if (type) {
        descriptorWrite.descriptorType = (vk::DescriptorType)type.value();
        goto found_type;
    } else {
        if (_data->layout) {
            auto layout = _data->layout;
            for (const auto& lb : layout->layoutBindings)
                if (lb.binding == binding) {
                    descriptorWrite.descriptorType = lb.descriptorType;
                    goto found_type;
                }
        }
    }
    throw VSL_NAMESPACE::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
found_type:

    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = nullptr;
    descriptorWrite.pImageInfo = &imageInfo;
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    _data->bindings[static_cast<std::uint32_t>(binding)] = {image._data, sampler._data};
    _data->pool->device->device.updateDescriptorSets({descriptorWrite}, {});
}

VSL_NAMESPACE::_impl::GraphicResource_impl::~GraphicResource_impl() {
    if (pool && pool->descriptorPool && descriptorSet)
        (void)pool->device->device.freeDescriptorSets(pool->descriptorPool, 1, &descriptorSet);
}
