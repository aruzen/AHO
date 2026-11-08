#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <VSL/exceptions.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    void require_descriptor_valid(const impl::GraphicResourceImpl& resource) {
        if (!resource.pool || !resource.pool->descriptor_pool || !resource.descriptor_set || resource.pool->retiring)
            throw std::logic_error("Descriptor resource has been destroyed");
    }
    void require_descriptor_idle(const impl::GraphicResourceImpl& resource) {
        require_descriptor_valid(resource);
        if (resource.recording)
            throw std::logic_error("Descriptor resource is being recorded");
        resource.use.require_idle();
        require_descriptor_valid(resource);
        if (resource.recording)
            throw std::logic_error("Descriptor resource is being recorded");
    }
    void require_descriptor_complete(const impl::GraphicResourceImpl& resource) {
        require_descriptor_valid(resource);
        if (!resource.layout)
            throw std::logic_error("Descriptor layout metadata is unavailable");
        for (const auto& point : resource.layout->layout_bindings) {
            const auto found = resource.bindings.find(point.binding);
            if (found == resource.bindings.end() || found->second.size() != point.descriptorCount ||
                !std::ranges::all_of(found->second, [](const auto& element) { return element.initialized; }))
                throw std::logic_error("Typed descriptor set has uninitialized bindings");
        }
    }
    void write_descriptor_binding(impl::GraphicResourceImpl& resource, vk::WriteDescriptorSet write,
                                  std::vector<impl::GraphicResourceImpl::BindingElement> elements) {
        require_descriptor_idle(resource);
        constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
        if (elements.empty() || elements.size() > maximum || write.dstArrayElement > maximum - elements.size())
            throw std::out_of_range("Descriptor update range is invalid");
        const auto end = write.dstArrayElement + elements.size();
        auto capacity = end;
        if (resource.layout) {
            const auto& points = resource.layout->layout_bindings;
            const auto found = std::ranges::find(points, write.dstBinding, &vk::DescriptorSetLayoutBinding::binding);
            if (found == points.end())
                throw exceptions::RuntimeException("BindingNotFound", "Descriptor binding not found!");
            if (found->descriptorType != write.descriptorType)
                throw std::invalid_argument("Descriptor update type does not match the layout");
            if (end > found->descriptorCount)
                throw std::out_of_range("Descriptor update exceeds the layout array capacity");
            capacity = found->descriptorCount;
        }
        const auto found = resource.bindings.find(write.dstBinding);
        auto prepared = found == resource.bindings.end() ? std::vector<impl::GraphicResourceImpl::BindingElement>{} : found->second;
        prepared.resize(std::max(capacity, prepared.size()));
        for (size_t i = 0; i < elements.size(); ++i) {
            elements[i].initialized = true;
            prepared[write.dstArrayElement + i] = std::move(elements[i]);
        }
        // mapの確保もnative呼び出し前に終える。失敗しても既存要素は変更しない。
        auto& target = resource.bindings.try_emplace(write.dstBinding).first->second;
        write.dstSet = resource.descriptor_set;
        write.descriptorCount = static_cast<std::uint32_t>(elements.size());
        operation::write_descriptor(resource.pool->device->device, write);
        target.swap(prepared);
    }
} // namespace VSL_NAMESPACE::lifetime
