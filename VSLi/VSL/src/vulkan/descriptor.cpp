//
// Created by morimoto_hibiki on 2025/06/09.
//
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/sampler_access.hpp>
#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/descriptor.hpp>
#include <VSL/lifetime/buffer_view.hpp>
#include <VSL/vulkan/descriptor_set_manager.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <limits>

#include <utility>
#include <ranges>

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
    using VSL_NAMESPACE::lifetime::detail::SamplerAccess;
} // namespace

namespace {
    void validate_pool(const std::shared_ptr<VSL_NAMESPACE::impl::GraphicResourcePoolImpl>& pool) {
        if (!pool || !pool->descriptor_pool || pool->retiring)
            throw std::logic_error("Descriptor pool has been destroyed");
        std::erase_if(pool->resources, [](const auto& resource) { return resource.expired(); });
    }
    void validate_layout(const std::shared_ptr<VSL_NAMESPACE::impl::GraphicResourcePoolImpl>& pool,
                         const VSL_NAMESPACE::graphic_resource::BindingLayout& layout) {
        if (!layout.data_ || layout.data_->device != pool->device)
            throw std::invalid_argument("Descriptor layout device mismatch");
    }
    void validate_resource(const std::shared_ptr<VSL_NAMESPACE::impl::GraphicResourceImpl>& resource) {
        if (!resource)
            throw std::logic_error("Descriptor resource has been destroyed");
        VSL_NAMESPACE::lifetime::require_descriptor_valid(*resource);
    }
} // namespace

namespace VSL_NAMESPACE::graphic_resource {
    namespace {
        void update_buffer(std::shared_ptr<impl::GraphicResourceImpl> target, lifetime::OwnedRef<impl::BufferImpl> buffer,
                           size_t binding, std::optional<Type> type, size_t offset, std::optional<size_t> size);
    }
} // namespace VSL_NAMESPACE::graphic_resource

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                              VSL_NAMESPACE::graphic_resource::Type binding_type,
                                                              VSL_NAMESPACE::ShaderFlag shader_type)
    : BindingLayout(device, 0, binding_type, shader_type, 1) {}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device, size_t binding,
                                                              Type binding_type, ShaderFlag shader_type, size_t amount) {
    if ((binding != size_t(-1) && binding > UINT32_MAX) || amount > UINT32_MAX)
        throw std::out_of_range("Descriptor binding/count exceeds uint32_t");
    const BindingPoint point{binding == size_t(-1) ? UINT32_MAX : static_cast<std::uint32_t>(binding), binding_type,
                             shader_type, static_cast<std::uint32_t>(amount)};
    data_ = lifetime::make_heap_descriptor_layout(device, {{point}});
}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                              const std::vector<BindingPoint>& points) {
    data_ = lifetime::make_heap_descriptor_layout(device, {points});
}

VSL_NAMESPACE::graphic_resource::BindingLayout::BindingLayout(LogicalDeviceAccessor device,
                                                              std::initializer_list<BindingPoint> points)
    : BindingLayout(device, std::vector<BindingPoint>(points.begin(), points.end())) {}

std::vector<VSL_NAMESPACE::graphic_resource::BindingPoint> VSL_NAMESPACE::graphic_resource::BindingLayout::get_binding_points() const {
    if (!data_ || !data_->layout)
        throw std::logic_error("Descriptor layout has been destroyed");
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

VSL_NAMESPACE::graphic_resource::ManagerInterface* VSL_NAMESPACE::graphic_resource::Pool::get_manager() {
    auto identity = data_ ? data_->manager.lock() : nullptr;
    return identity ? identity->manager : nullptr;
}

std::tuple<bool, VSL_NAMESPACE::graphic_resource::Resource>
VSL_NAMESPACE::graphic_resource::Pool::bind(VSL_NAMESPACE::graphic_resource::BindingLayout layout) {
    validate_pool(data_);
    validate_layout(data_, layout);
    const std::array layouts{layout.data_};
    auto [result, resources] = lifetime::make_heap_descriptor_sets(data_, layouts);
    if (result != vk::Result::eSuccess)
        return {false, {}};
    return {true, {std::move(resources.front())}};
}

std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::Pool::bind(size_t n, VSL_NAMESPACE::graphic_resource::BindingLayout layout) {
    validate_pool(data_);
    validate_layout(data_, layout);
    if (n > UINT32_MAX)
        throw std::length_error("Descriptor allocation count exceeds uint32_t");
    return bind(std::vector<BindingLayout>(n, layout));
}

std::tuple<bool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::Pool::bind(std::vector<VSL_NAMESPACE::graphic_resource::BindingLayout> layouts) {
    validate_pool(data_);
    for (const auto& layout : layouts)
        validate_layout(data_, layout);
    std::vector<Resource> resources(layouts.size());
    const auto refs = layouts | std::views::transform([](const auto& layout) { return layout.data_; }) | std::ranges::to<std::vector>();
    auto [result, allocated] = lifetime::make_heap_descriptor_sets(data_, refs);
    if (result != vk::Result::eSuccess)
        return {false, {}};
    for (size_t i = 0; i < resources.size(); ++i)
        resources[i].data_ = std::move(allocated[i]);
    return {true, std::move(resources)};
}

VSL_NAMESPACE::graphic_resource::Pool VSL_NAMESPACE::graphic_resource::Resource::get_pool() {
    const auto resource = data_;
    if (!resource || !resource->pool)
        throw std::logic_error("Descriptor resource is empty");
    auto owner = resource->pool->owner.lock();
    if (!owner.use_count())
        throw std::logic_error("Descriptor pool owner is unavailable");
    return {{std::move(owner), resource->pool}};
}
VSL_NAMESPACE::DescriptorIdentity VSL_NAMESPACE::graphic_resource::Resource::identity() const {
    return lifetime::detail::IdentityAccess::From<DescriptorIdentity>(data_.get());
}

VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::graphic_resource::Resource::owner_ref() const {
    return lifetime::detail::owned_ref(data_, data_ ? data_->owner_node : nullptr);
}

void VSL_NAMESPACE::graphic_resource::Resource::update(const BufferView& buffer, size_t binding, std::optional<Type> type,
                                                       size_t offset, std::optional<size_t> size) {
    const auto range = buffer.view(offset, size.value_or(buffer.size() - offset));
    update_buffer(data_, lifetime::detail::BufferAccess::Owned(range), binding, type, range.byte_offset(), range.size());
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::BufferAccessor* buffer, size_t binding, std::optional<Type> type,
                                                       size_t offset, std::optional<size_t> size) {
    const auto data = buffer ? BufferAccess::Owned(*(buffer)) : nullptr;
    update_buffer(data_, data, binding, type, offset, size);
}

namespace VSL_NAMESPACE::graphic_resource {
    namespace {
        void update_buffer(std::shared_ptr<impl::GraphicResourceImpl> target, lifetime::OwnedRef<impl::BufferImpl> buffer,
                           size_t binding, std::optional<Type> type, size_t offset, std::optional<size_t> size) {
            validate_resource(target);
            if (!buffer || buffer->device != target->pool->device)
                throw std::invalid_argument("Descriptor buffer device mismatch");
            const auto buffer_size = buffer->requirement_size;
            if (offset >= buffer_size)
                throw VSL_NAMESPACE::exceptions::RuntimeException("BufferRange", "Buffer descriptor range starts outside the buffer");
            const vk::DeviceSize range = size ? *size : buffer_size - offset;
            if (range == 0 || (range != VK_WHOLE_SIZE && range > buffer_size - offset))
                throw VSL_NAMESPACE::exceptions::RuntimeException("BufferRange", "Buffer descriptor range is empty or exceeds the buffer");

            vk::DescriptorBufferInfo buffer_info;
            buffer_info.buffer = buffer->buffer;
            buffer_info.offset = offset;
            buffer_info.range = range;

            vk::WriteDescriptorSet descriptor_write;
            descriptor_write.dstSet = target->descriptor_set;
            descriptor_write.dstBinding = binding;
            descriptor_write.dstArrayElement = 0;
            if (type)
                descriptor_write.descriptorType = (vk::DescriptorType)type.value();
            else {
                const auto layout = target->layout;
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

            operation::validate_descriptor_buffer(*buffer, descriptor_write.descriptorType, offset, range);
            lifetime::write_descriptor_binding(*target, descriptor_write, {{{buffer}, {buffer->use}, {}}});
        }
    } // namespace
} // namespace VSL_NAMESPACE::graphic_resource

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::ImageAccessor image, size_t binding, std::optional<Type> type) {
    const auto target = data_;
    validate_resource(target);
    if (!ImageAccess::Owned(image) || ImageAccess::Owned(image)->device != target->pool->device)
        throw std::invalid_argument("Descriptor image device mismatch");
    vk::DescriptorImageInfo image_info;
    image_info.imageView = ImageAccess::Owned(image)->view;
    image_info.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet descriptor_write;
    descriptor_write.dstSet = target->descriptor_set;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = 0;
    if (type) {
        descriptor_write.descriptorType = (vk::DescriptorType)type.value();
        goto found_type;
    } else {
        if (target->layout) {
            auto layout = target->layout;
            for (const auto& lb : layout->layout_bindings)
                if (lb.binding == binding) {
                    descriptor_write.descriptorType = lb.descriptorType;
                    goto found_type;
                }
        }
    }
    throw VSL_NAMESPACE::exceptions::RuntimeException("LayoutNotFound", "Layout not found!", std::source_location::current());
found_type:

    if (descriptor_write.descriptorType == vk::DescriptorType::eCombinedImageSampler)
        throw std::invalid_argument("Combined image descriptor requires a sampler");

    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = nullptr;
    descriptor_write.pImageInfo = &image_info;
    descriptor_write.pTexelBufferView = nullptr; // Optional

    operation::validate_descriptor_image(*ImageAccess::Owned(image), descriptor_write.descriptorType);
    lifetime::write_descriptor_binding(*target, descriptor_write, {{{ImageAccess::Owned(image)}, {ImageAccess::Owned(image)->use}, {}}});
}

void VSL_NAMESPACE::graphic_resource::Resource::update(VSL_NAMESPACE::ImageAccessor image, SamplerAccessor sampler,
                                                       size_t binding, std::optional<Type> type) {
    const auto target = data_;
    validate_resource(target);
    if (!ImageAccess::Owned(image) || ImageAccess::Owned(image)->device != target->pool->device)
        throw std::invalid_argument("Descriptor image device mismatch");
    vk::DescriptorImageInfo image_info;
    image_info.imageView = ImageAccess::Owned(image)->view;
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    if (!SamplerAccess::Owned(sampler) || !SamplerAccess::Owned(sampler)->sampler || !SamplerAccess::Owned(sampler)->use || SamplerAccess::Owned(sampler)->device != target->pool->device)
        throw std::invalid_argument("Descriptor sampler device mismatch");
    image_info.sampler = SamplerAccess::Owned(sampler)->sampler;

    vk::WriteDescriptorSet descriptor_write;
    descriptor_write.dstSet = target->descriptor_set;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = 0;
    if (type) {
        descriptor_write.descriptorType = (vk::DescriptorType)type.value();
        goto found_type;
    } else {
        if (target->layout) {
            auto layout = target->layout;
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

    if (descriptor_write.descriptorType == vk::DescriptorType::eSampler) {
        lifetime::write_descriptor_binding(*target, descriptor_write, {{{SamplerAccess::Owned(sampler)}, {SamplerAccess::Owned(sampler)->use}, {}}});
        return;
    }
    operation::validate_descriptor_image(*ImageAccess::Owned(image), descriptor_write.descriptorType);
    lifetime::write_descriptor_binding(*target, descriptor_write,
                                       {{{ImageAccess::Owned(image), SamplerAccess::Owned(sampler)}, {ImageAccess::Owned(image)->use, SamplerAccess::Owned(sampler)->use}, {}}});
}
