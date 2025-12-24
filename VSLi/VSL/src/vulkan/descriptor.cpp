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
    for (size_t i = 0; i < points.size(); i++) {
        auto &point = points[i];
        _data->layoutBindings[i].binding = point.binding == (std::uint32_t) -1 ? (std::uint32_t) i : point.binding;
        _data->layoutBindings[i].descriptorType = (vk::DescriptorType) point.bindingType;
        _data->layoutBindings[i].stageFlags = (vk::ShaderStageFlagBits) point.shaderType;
        _data->layoutBindings[i].descriptorCount = point.amount;
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
        result[i].binding = binding.binding == (std::uint32_t) -1 ? (std::uint32_t) i : binding.binding;
        result[i].bindingType = (Type) binding.descriptorType;
        result[i].shaderType = (ShaderFlag)((uint32_t) binding.stageFlags);
        result[i].amount = binding.descriptorCount;
    }
    return result;
}

vsl::_impl::ResourceBindingLayout_impl::~ResourceBindingLayout_impl() {
    device->device.destroyDescriptorSetLayout(layout);
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
vsl::graphic_resource::Pool::bind(size_t n, vsl::graphic_resource::BindingLayout layout) {
    std::tuple<bool, std::vector<vsl::graphic_resource::Resource>> result{true, {}};
    auto &[check, resources] = result;
    resources.resize(n);

    auto vkLayouts = std::vector(n, layout._data->layout);

    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = _data->descriptorPool;
    allocInfo.descriptorSetCount = vkLayouts.size();
    allocInfo.pSetLayouts = vkLayouts.data();

    std::vector<vk::DescriptorSet> descriptorSets;
    descriptorSets.resize(vkLayouts.size());
    if (((ManagerInterface *) _data->manager)->_data->device->device.
            allocateDescriptorSets(&allocInfo, descriptorSets.data()) != vk::Result::eSuccess)
        return {false, {}};
    for (size_t i = 0; i < descriptorSets.size(); i++) {
        resources[i] = Resource{std::make_shared<_impl::GraphicResource_impl>()};
        resources[i]._data->pool = _data;
        resources[i]._data->descriptorSet = descriptorSets[i];
        _data->resources.push_back(resources[i]._data);
    }
    return result;
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

void vsl::graphic_resource::Resource::update(vsl::ImageAccessor image, size_t binding, std::optional<Type> type) {
    vk::DescriptorImageInfo imageInfo;
    imageInfo.imageView = image._data->view;
    imageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet descriptorWrite;
    descriptorWrite.dstSet = this->_data->descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    if (type) {
        descriptorWrite.descriptorType = (vk::DescriptorType) type.value();
        goto found_type;
    } else {
        if (not _data->layout.expired()) {
            auto layout = _data->layout.lock();
            for (const auto &lb: layout->layoutBindings)
                if (lb.binding == binding) {
                    descriptorWrite.descriptorType = lb.descriptorType;
                    goto found_type;
                }
        }
    }
    throw vsl::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
    found_type:

    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = nullptr;
    descriptorWrite.pImageInfo = &imageInfo;
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    ((ManagerInterface *) _data->pool->manager)->_data->device->device.updateDescriptorSets({descriptorWrite}, {});
}

void vsl::graphic_resource::Resource::update(vsl::ImageAccessor image, SamplerAccessor sampler,
                                             size_t binding, std::optional<Type> type) {
    vk::DescriptorImageInfo imageInfo;
    imageInfo.imageView = image._data->view;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.sampler = sampler._data->sampler;

    vk::WriteDescriptorSet descriptorWrite;
    descriptorWrite.dstSet = this->_data->descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    if (type) {
        descriptorWrite.descriptorType = (vk::DescriptorType) type.value();
        goto found_type;
    } else {
        if (not _data->layout.expired()) {
            auto layout = _data->layout.lock();
            for (const auto &lb: layout->layoutBindings)
                if (lb.binding == binding) {
                    descriptorWrite.descriptorType = lb.descriptorType;
                    goto found_type;
                }
        }
    }
    throw vsl::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
    found_type:

    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = nullptr;
    descriptorWrite.pImageInfo = &imageInfo;
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    ((ManagerInterface *) _data->pool->manager)->_data->device->device.updateDescriptorSets({descriptorWrite}, {});
}
