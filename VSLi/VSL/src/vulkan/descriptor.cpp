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
    void validate_pool(const std::shared_ptr<VSL_NAMESPACE::impl::GraphicResourcePoolImpl>& pool) {
        if (!pool || !pool->descriptor_pool)
            throw std::logic_error("Descriptor pool has been destroyed");
        std::erase_if(pool->resources, [](const auto& resource) { return resource.expired(); });
    }
    void validate_layout(const std::shared_ptr<VSL_NAMESPACE::impl::GraphicResourcePoolImpl>& pool,
                         const VSL_NAMESPACE::graphic_resource::BindingLayout& layout) {
        if (!layout.data_ || layout.data_->device != pool->device)
            throw std::invalid_argument("Descriptor layout device mismatch");
    }
    void validate_resource(const std::shared_ptr<VSL_NAMESPACE::impl::GraphicResourceImpl>& resource) {
        if (!resource || !resource->pool || !resource->pool->descriptor_pool || !resource->descriptor_set)
            throw std::logic_error("Descriptor resource has been destroyed");
    }
} // namespace

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                              VSL_NAMESPACE::graphic_resource::Type binding_type,
                                                              VSL_NAMESPACE::ShaderFlag shader_type)
    : BindingLayout(device, 0, binding_type, shader_type, 1) {}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device, size_t binding,
                                                              Type binding_type, ShaderFlag shader_type, size_t amount) {
    data_ = std::make_shared<impl::ResourceBindingLayoutImpl>();
    data_->device = device.data_;

    data_->layout_bindings.resize(1);
    data_->layout_bindings[0].binding = binding == size_t(-1) || binding == UINT32_MAX ? 0 : binding;
    data_->layout_bindings[0].descriptorType = (vk::DescriptorType)binding_type;
    data_->layout_bindings[0].stageFlags = (vk::ShaderStageFlagBits)shader_type;
    data_->layout_bindings[0].descriptorCount = amount;

    vk::DescriptorSetLayoutCreateInfo layout_create_info;
    layout_create_info.bindingCount = 1;
    layout_create_info.pBindings = data_->layout_bindings.data();

    data_->layout = device.data_->device.createDescriptorSetLayout(layout_create_info);
}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                              const std::vector<BindingPoint>& points) {
    data_ = std::make_shared<impl::ResourceBindingLayoutImpl>();
    data_->device = device.data_;

    data_->layout_bindings.resize(points.size());
    for (size_t i = 0; i < points.size(); i++) {
        auto& point = points[i];
        data_->layout_bindings[i].binding = point.binding == (std::uint32_t)-1 ? (std::uint32_t)i : point.binding;
        data_->layout_bindings[i].descriptorType = (vk::DescriptorType)point.binding_type;
        data_->layout_bindings[i].stageFlags = (vk::ShaderStageFlagBits)point.shader_type;
        data_->layout_bindings[i].descriptorCount = point.amount;
    }

    vk::DescriptorSetLayoutCreateInfo layout_create_info;
    layout_create_info.bindingCount = data_->layout_bindings.size();
    layout_create_info.pBindings = data_->layout_bindings.data();

    data_->layout = data_->device->device.createDescriptorSetLayout(layout_create_info);
}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device,
                                                              std::initializer_list<BindingPoint> points)
    : BindingLayout(device, std::vector<BindingPoint>(points.begin(), points.end())) {}

std::vector<VSL_NAMESPACE::graphic_resource::BindingPoint> VSL_NAMESPACE::graphic_resource::BindingLayout::get_binding_points() const {
    std::vector<BindingPoint> result;

    result.resize(this->data_->layout_bindings.size());
    for (size_t i = 0; i < result.size(); i++) {
        auto& binding = this->data_->layout_bindings[i];
        result[i].binding = binding.binding == (std::uint32_t)-1 ? (std::uint32_t)i : binding.binding;
        result[i].binding_type = (Type)binding.descriptorType;
        result[i].shader_type = (ShaderFlag)((uint32_t)binding.stageFlags);
        result[i].amount = binding.descriptorCount;
    }
    return result;
}

VSL_NAMESPACE::impl::ResourceBindingLayoutImpl::~ResourceBindingLayoutImpl() {
    device->device.destroyDescriptorSetLayout(layout);
}

VSL_NAMESPACE::graphic_resource::ManagerInterface* VSL_NAMESPACE::graphic_resource::Pool::get_manager() {
    auto identity = data_ ? data_->manager.lock() : nullptr;
    return identity ? identity->manager : nullptr;
}

std::tuple<bool, VSL_NAMESPACE::graphic_resource::Resource>
VSL_NAMESPACE::graphic_resource::Pool::bind(VSL_NAMESPACE::graphic_resource::BindingLayout layout) {
    validate_pool(data_);
    validate_layout(data_, layout);
    auto result = std::make_shared<impl::GraphicResourceImpl>();
    result->pool = data_;
    result->layout = layout.data_;
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = data_->descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout.data_->layout;

    if (data_->device->device.allocateDescriptorSets(&alloc_info, &result->descriptor_set) != vk::Result::eSuccess) {
        return {false, {}};
    }
    data_->resources.push_back(result);
    return {true, {result}};
}

std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::Pool::bind(size_t n, VSL_NAMESPACE::graphic_resource::BindingLayout layout) {
    validate_pool(data_);
    validate_layout(data_, layout);
    if (n == 0)
        return {true, {}};
    std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>> result{true, {}};
    auto& [check, resources] = result;
    resources.resize(n);

    auto vk_layouts = std::vector(n, layout.data_->layout);

    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = data_->descriptor_pool;
    alloc_info.descriptorSetCount = vk_layouts.size();
    alloc_info.pSetLayouts = vk_layouts.data();

    std::vector<vk::DescriptorSet> descriptor_sets;
    descriptor_sets.resize(vk_layouts.size());
    if (data_->device->device.allocateDescriptorSets(&alloc_info, descriptor_sets.data()) != vk::Result::eSuccess)
        return {false, {}};
    for (size_t i = 0; i < descriptor_sets.size(); i++) {
        resources[i] = Resource{std::make_shared<impl::GraphicResourceImpl>()};
        resources[i].data_->pool = data_;
        resources[i].data_->layout = layout.data_;
        resources[i].data_->descriptor_set = descriptor_sets[i];
        data_->resources.push_back(resources[i].data_);
    }
    return result;
}

std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::Pool::bind(std::vector<VSL_NAMESPACE::graphic_resource::BindingLayout> layouts) {
    validate_pool(data_);
    for (const auto& layout : layouts)
        validate_layout(data_, layout);
    if (layouts.empty())
        return {true, {}};
    std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>> result{true, {}};
    auto& [check, resources] = result;
    resources.resize(layouts.size());

    for (size_t i = 0; i < resources.size(); i++) {
        auto& r = resources[i];
        r.data_ = std::make_shared<impl::GraphicResourceImpl>();
        r.data_->pool = this->data_;
        r.data_->layout = layouts[i].data_;
    }

    auto vk_layouts = layouts | std::views::transform([](const BindingLayout& layout) { return layout.data_->layout; }) | std::ranges::to<std::vector>();
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = data_->descriptor_pool;
    alloc_info.descriptorSetCount = vk_layouts.size();
    alloc_info.pSetLayouts = vk_layouts.data();

    std::vector<vk::DescriptorSet> descriptor_sets(vk_layouts.size());
    if (data_->device->device.allocateDescriptorSets(&alloc_info, descriptor_sets.data()) != vk::Result::eSuccess)
        return {false, {}};
    for (size_t i = 0; i < descriptor_sets.size(); i++) {
        resources[i].data_->descriptor_set = descriptor_sets[i];
        data_->resources.push_back(resources[i].data_);
    }
    return result;
}

VSL_NAMESPACE::impl::GraphicResourcePoolImpl::~GraphicResourcePoolImpl() {
    device->device.destroyDescriptorPool(descriptor_pool);
}

VSL_NAMESPACE::graphic_resource::Pool VSL_NAMESPACE::graphic_resource::Resource::get_pool() {
    return {data_->pool};
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::BufferAccessor* buffer, size_t binding, std::optional<Type> type,
                                                       size_t offset, std::optional<size_t> size) {
    validate_resource(data_);
    if (!buffer || !buffer->data_ || buffer->data_->device != data_->pool->device)
        throw std::invalid_argument("Descriptor buffer device mismatch");
    const auto buffer_size = buffer->data_->requirement_size;
    if (offset >= buffer_size)
        throw VSL_NAMESPACE::exceptions::RuntimeException("BufferRange", "Buffer descriptor range starts outside the buffer");
    const vk::DeviceSize range = size ? *size : buffer_size - offset;
    if (range == 0 || (range != VK_WHOLE_SIZE && range > buffer_size - offset))
        throw VSL_NAMESPACE::exceptions::RuntimeException("BufferRange", "Buffer descriptor range is empty or exceeds the buffer");

    vk::DescriptorBufferInfo buffer_info;
    buffer_info.buffer = buffer->data_->buffer;
    buffer_info.offset = offset;
    buffer_info.range = range;

    vk::WriteDescriptorSet descriptor_write;
    descriptor_write.dstSet = this->data_->descriptor_set;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = 0;
    if (type)
        descriptor_write.descriptorType = (vk::DescriptorType)type.value();
    else {
        const auto layout = data_->layout;
        if (!layout)
            throw VSL_NAMESPACE::exceptions::RuntimeException("LayoutNotFound", "Layout not found!",
                                                              std::source_location::current());
        const auto found = std::ranges::find(layout->layout_bindings, binding,
                                             &vk::DescriptorSetLayoutBinding::binding);
        if (found == layout->layout_bindings.end())
            throw VSL_NAMESPACE::exceptions::RuntimeException("BindingNotFound", "Descriptor binding not found!");
        descriptor_write.descriptorType = found->descriptorType;
    }
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &buffer_info;
    descriptor_write.pImageInfo = nullptr;       // Optional
    descriptor_write.pTexelBufferView = nullptr; // Optional

    auto owners = std::vector<std::shared_ptr<void>>{buffer->data_};
    data_->bindings[static_cast<std::uint32_t>(binding)] = std::move(owners);
    data_->pool->device->device.updateDescriptorSets({descriptor_write}, {});
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::ImageAccessor image, size_t binding, std::optional<Type> type) {
    validate_resource(data_);
    if (!image.data_ || image.data_->device != data_->pool->device)
        throw std::invalid_argument("Descriptor image device mismatch");
    vk::DescriptorImageInfo image_info;
    image_info.imageView = image.data_->view;
    image_info.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet descriptor_write;
    descriptor_write.dstSet = this->data_->descriptor_set;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = 0;
    if (type) {
        descriptor_write.descriptorType = (vk::DescriptorType)type.value();
        goto found_type;
    } else {
        if (data_->layout) {
            auto layout = data_->layout;
            for (const auto& lb : layout->layout_bindings)
                if (lb.binding == binding) {
                    descriptor_write.descriptorType = lb.descriptorType;
                    goto found_type;
                }
        }
    }
    throw VSL_NAMESPACE::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
found_type:

    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = nullptr;
    descriptor_write.pImageInfo = &image_info;
    descriptor_write.pTexelBufferView = nullptr; // Optional

    data_->bindings[static_cast<std::uint32_t>(binding)] = {image.data_};
    data_->pool->device->device.updateDescriptorSets({descriptor_write}, {});
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::ImageAccessor image, SamplerAccessor sampler,
                                                       size_t binding, std::optional<Type> type) {
    validate_resource(data_);
    if (!image.data_ || image.data_->device != data_->pool->device)
        throw std::invalid_argument("Descriptor image device mismatch");
    vk::DescriptorImageInfo image_info;
    image_info.imageView = image.data_->view;
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    if (!sampler.data_ || sampler.data_->device != data_->pool->device)
        throw std::invalid_argument("Descriptor sampler device mismatch");
    image_info.sampler = sampler.data_->sampler;

    vk::WriteDescriptorSet descriptor_write;
    descriptor_write.dstSet = this->data_->descriptor_set;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = 0;
    if (type) {
        descriptor_write.descriptorType = (vk::DescriptorType)type.value();
        goto found_type;
    } else {
        if (data_->layout) {
            auto layout = data_->layout;
            for (const auto& lb : layout->layout_bindings)
                if (lb.binding == binding) {
                    descriptor_write.descriptorType = lb.descriptorType;
                    goto found_type;
                }
        }
    }
    throw VSL_NAMESPACE::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
found_type:

    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = nullptr;
    descriptor_write.pImageInfo = &image_info;
    descriptor_write.pTexelBufferView = nullptr; // Optional

    data_->bindings[static_cast<std::uint32_t>(binding)] = {image.data_, sampler.data_};
    data_->pool->device->device.updateDescriptorSets({descriptor_write}, {});
}

VSL_NAMESPACE::impl::GraphicResourceImpl::~GraphicResourceImpl() {
    if (pool && pool->descriptor_pool && descriptor_set)
        (void)pool->device->device.freeDescriptorSets(pool->descriptor_pool, 1, &descriptor_set);
}
