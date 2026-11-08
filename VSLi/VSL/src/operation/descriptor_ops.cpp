#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <VSL/operation/descriptor_create_info.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    void validate_descriptor_layout(const impl::ResourceBindingLayoutImpl& layout, const impl::LogicalDeviceImpl& device,
                                    std::span<const graphic_resource::BindingPoint> points) {
        if (!layout.layout || layout.device != &device || layout.layout_bindings.size() != points.size())
            throw std::invalid_argument("Typed descriptor device/layout mismatch");
        for (const auto& point : points) {
            const auto found = std::ranges::find(layout.layout_bindings, point.binding, &vk::DescriptorSetLayoutBinding::binding);
            if (found == layout.layout_bindings.end() || found->descriptorType != vk::DescriptorType(point.binding_type) ||
                found->descriptorCount != point.amount || found->stageFlags != vk::ShaderStageFlags(vk::ShaderStageFlagBits(point.shader_type)) ||
                found->pImmutableSamplers)
                throw std::invalid_argument("Typed descriptor binding mismatch");
        }
    }
    void initialize_descriptor_layout(impl::ResourceBindingLayoutImpl& layout, impl::LogicalDeviceImpl& device, const DescriptorLayoutCreateInfo& info) {
        layout.device = &device;
        layout.native_device = device.device;
        if (info.bindings.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Descriptor layout binding count exceeds uint32_t");
        layout.layout_bindings.reserve(info.bindings.size());
        for (size_t i = 0; i < info.bindings.size(); ++i) {
            const auto& point = info.bindings[i];
            const auto binding = point.binding == UINT32_MAX ? static_cast<std::uint32_t>(i) : point.binding;
            if (std::ranges::any_of(layout.layout_bindings, [&](const auto& value) { return value.binding == binding; }))
                throw std::invalid_argument("Descriptor layout requires unique binding numbers");
            layout.layout_bindings.push_back({.binding = binding, .descriptorType = vk::DescriptorType(point.binding_type), .descriptorCount = point.amount, .stageFlags = vk::ShaderStageFlagBits(point.shader_type)});
        }
        const vk::DescriptorSetLayoutCreateInfo create{.bindingCount = static_cast<std::uint32_t>(layout.layout_bindings.size()),
                                                       .pBindings = layout.layout_bindings.data()};
        layout.layout = device.device.createDescriptorSetLayout(create);
    }
    void destroy_descriptor_layout(impl::ResourceBindingLayoutImpl& layout) noexcept {
        if (layout.layout)
            layout.native_device.destroyDescriptorSetLayout(layout.layout);
        layout.layout = nullptr;
    }
    void initialize_descriptor_pool(impl::GraphicResourcePoolImpl& pool, impl::LogicalDeviceImpl& device, const DescriptorPoolCreateInfo& info) {
        if (!info.max_sets || info.max_sets > std::numeric_limits<std::uint32_t>::max())
            throw std::out_of_range("Descriptor pool maxSets must fit nonzero uint32_t");
        pool.device = &device;
        pool.native_device = device.device;
        pool.max_sets = static_cast<std::uint32_t>(info.max_sets);
        std::vector<vk::DescriptorPoolSize> sizes;
        sizes.reserve(info.descriptors.size());
        for (const auto& [type, count] : info.descriptors) {
            if (count > std::numeric_limits<std::uint32_t>::max())
                throw std::out_of_range("Descriptor pool capacity exceeds uint32_t");
            if (count) {
                sizes.push_back({vk::DescriptorType(type), static_cast<std::uint32_t>(count)});
                pool.capacities.emplace(vk::DescriptorType(type), impl::GraphicResourcePoolImpl::Capacity{static_cast<std::uint32_t>(count), 0});
            }
        }
        const vk::DescriptorPoolCreateInfo create{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                                  .maxSets = static_cast<std::uint32_t>(info.max_sets),
                                                  .poolSizeCount = static_cast<std::uint32_t>(sizes.size()),
                                                  .pPoolSizes = sizes.data()};
        pool.descriptor_pool = device.device.createDescriptorPool(create);
    }
    void destroy_descriptor_pool(impl::GraphicResourcePoolImpl& pool) noexcept {
        if (pool.descriptor_pool)
            pool.native_device.destroyDescriptorPool(pool.descriptor_pool);
        pool.descriptor_pool = nullptr;
        pool.max_sets = pool.allocated_sets = 0;
        for (auto& [type, capacity] : pool.capacities)
            capacity.allocated = 0;
    }
    vk::Result allocate_descriptor_sets(impl::GraphicResourcePoolImpl& pool, std::span<impl::GraphicResourceImpl* const> sets) {
        if (!pool.descriptor_pool || pool.retiring)
            throw std::logic_error("Descriptor pool has been destroyed");
        if (sets.empty() || sets.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Descriptor allocation count must fit nonzero uint32_t");
        std::vector<vk::DescriptorSetLayout> layouts;
        std::vector<vk::DescriptorSet> handles(sets.size());
        layouts.reserve(sets.size());
        for (size_t i = 0; i < sets.size(); ++i) {
            const auto* resource = sets[i];
            if (!resource || resource->descriptor_set || resource->pool != &pool || !resource->layout ||
                !resource->layout->layout || resource->layout->device != pool.device ||
                resource->allocation_layout != resource->layout || resource->native_device != pool.native_device ||
                std::ranges::contains(sets.first(i), resource))
                throw std::invalid_argument("Descriptor allocation requires prepared sets on the pool device");
            layouts.push_back(resource->layout->layout);
        }
        if (pool.allocated_sets > pool.max_sets || sets.size() > pool.max_sets - pool.allocated_sets)
            return vk::Result::eErrorOutOfPoolMemory;
        std::map<vk::DescriptorType, std::uint32_t> required;
        for (const auto* resource : sets)
            for (const auto& point : resource->layout->layout_bindings) {
                if (!point.descriptorCount)
                    continue;
                auto& count = required[point.descriptorType];
                if (point.descriptorCount > UINT32_MAX - count)
                    return vk::Result::eErrorOutOfPoolMemory;
                count += point.descriptorCount;
            }
        for (const auto& [type, count] : required) {
            const auto found = pool.capacities.find(type);
            if (found == pool.capacities.end() || found->second.allocated > found->second.limit || count > found->second.limit - found->second.allocated)
                return vk::Result::eErrorOutOfPoolMemory;
        }
        const vk::DescriptorSetAllocateInfo allocate{.descriptorPool = pool.descriptor_pool,
                                                     .descriptorSetCount = static_cast<std::uint32_t>(sets.size()),
                                                     .pSetLayouts = layouts.data()};
        const auto result = pool.native_device.allocateDescriptorSets(&allocate, handles.data());
        if (result == vk::Result::eSuccess) {
            for (size_t i = 0; i < sets.size(); ++i)
                sets[i]->descriptor_set = handles[i];
            pool.allocated_sets += static_cast<std::uint32_t>(sets.size());
            for (const auto& [type, count] : required)
                pool.capacities.find(type)->second.allocated += count;
        }
        return result;
    }
    void destroy_descriptor_set(impl::GraphicResourceImpl& resource) noexcept {
        if (resource.descriptor_set && resource.pool && resource.pool->descriptor_pool) {
            const auto result = resource.native_device.freeDescriptorSets(resource.pool->descriptor_pool, 1, &resource.descriptor_set);
            if (result == vk::Result::eSuccess) {
                if (resource.pool->allocated_sets)
                    --resource.pool->allocated_sets;
                if (resource.allocation_layout)
                    for (const auto& point : resource.allocation_layout->layout_bindings) {
                        const auto found = resource.pool->capacities.find(point.descriptorType);
                        if (found != resource.pool->capacities.end() && found->second.allocated >= point.descriptorCount)
                            found->second.allocated -= point.descriptorCount;
                    }
            }
        }
        resource.descriptor_set = nullptr;
    }
    void write_descriptor(vk::Device device, const vk::WriteDescriptorSet& write) {
        device.updateDescriptorSets(write, {});
    }
    void validate_descriptor_buffer(const impl::BufferImpl& buffer, vk::DescriptorType type, size_t offset, size_t bytes) {
        const bool uniform = type == vk::DescriptorType::eUniformBuffer || type == vk::DescriptorType::eUniformBufferDynamic;
        const bool storage = type == vk::DescriptorType::eStorageBuffer || type == vk::DescriptorType::eStorageBufferDynamic;
        if (!buffer.buffer || !buffer.use || (!uniform && !storage) ||
            !contain(buffer.usage, uniform ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER))
            throw std::invalid_argument("Descriptor buffer usage/type mismatch");
        if (!bytes || offset >= buffer.requirement_size || (bytes != VK_WHOLE_SIZE && bytes > buffer.requirement_size - offset))
            throw std::out_of_range("Descriptor buffer range mismatch");
        const auto limits = buffer.device->parent_device->device.getProperties().limits;
        const auto limit = uniform ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
        const auto effective_range = bytes == VK_WHOLE_SIZE ? buffer.requirement_size - offset : bytes;
        if (effective_range > limit)
            throw std::out_of_range("Descriptor buffer exceeds the device range limit");
        const auto alignment = uniform ? limits.minUniformBufferOffsetAlignment : limits.minStorageBufferOffsetAlignment;
        if (alignment && offset % alignment)
            throw std::invalid_argument("Descriptor buffer offset violates device alignment");
    }
    void validate_descriptor_image(const impl::ImageImpl& image, vk::DescriptorType type) {
        auto usage = ImageType::SAMPLED;
        if (type == vk::DescriptorType::eStorageImage)
            usage = ImageType::STORAGE;
        else if (type == vk::DescriptorType::eInputAttachment)
            usage = ImageType::INPUT_ATTACHMENT;
        else if (type != vk::DescriptorType::eSampledImage && type != vk::DescriptorType::eCombinedImageSampler)
            throw std::invalid_argument("Descriptor image type mismatch");
        if (!image.view || !image.use || !contain(image.usage, usage))
            throw std::invalid_argument("Descriptor image usage mismatch");
    }
} // namespace VSL_NAMESPACE::operation
