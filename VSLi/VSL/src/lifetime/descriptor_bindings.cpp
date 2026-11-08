#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <VSL/exceptions.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct DescriptorMutation {
            impl::GraphicResourceImpl& resource;
            bool active = true;
            explicit DescriptorMutation(impl::GraphicResourceImpl& resource);
            ~DescriptorMutation() { finish(); }
            void check(std::span<const impl::GraphicResourceImpl::BindingElement> elements) const;
            void finish() noexcept;
        };
        void visit_elements(std::span<const impl::GraphicResourceImpl::BindingElement> elements, DependencyNode::EdgeVisitor visitor, void* context);
        void visit_elements(std::span<const impl::GraphicResourceImpl::BindingElement> elements, DependencyNode::EdgeVisitor visitor, void* context) {
            for (const auto& element : elements) {
                for (const auto& owner : element.owners)
                    visitor(context, owner);
                for (const auto& owner : element.constraint.owners)
                    visitor(context, owner);
            }
        }
        DescriptorMutation::DescriptorMutation(impl::GraphicResourceImpl& resource) : resource(resource) {
            if (resource.recording)
                throw std::logic_error("Descriptor resource is already being changed");
            if (resource.owner_context)
                resource.owner_context->node.begin_update();
            resource.recording = true;
        }
        void DescriptorMutation::check(std::span<const impl::GraphicResourceImpl::BindingElement> elements) const {
            resource.use.require_idle();
            require_descriptor_valid(resource);
            if (resource.owner_context)
                resource.owner_context->node.check_additions(&elements, [](const void* input, DependencyNode::EdgeVisitor visitor, void* context) {
                    visit_elements(*static_cast<const std::span<const impl::GraphicResourceImpl::BindingElement>*>(input), visitor, context);
                });
        }
        void DescriptorMutation::finish() noexcept {
            if (!active)
                return;
            active = false;
            resource.recording = false;
            if (resource.owner_context)
                resource.owner_context->node.end_update();
        }
    } // namespace
    void DescriptorOwnerContext::normalize(std::vector<OwnerRef>& owners) const {
        const auto borrowed = [&](const auto& owner) {
            const auto same = [&](const auto& known) { return !known.owner_before(owner) && !owner.owner_before(known); };
            return node.matches(detail::OwnerAccess::Shared(owner)) ||
                   (device_parent && device_parent->matches(detail::OwnerAccess::Shared(owner))) || std::ranges::any_of(dependencies, same);
        };
        for (const auto& owner : owners) {
            if (owner.use_count() && !borrowed(owner) && policy == DependencyPolicy::FIXED)
                throw std::invalid_argument("Descriptor arena requires a declared external owner");
        }
        std::erase_if(owners, borrowed);
    }
    void visit_descriptor_edges(const impl::GraphicResourceImpl& resource, DependencyNode::EdgeVisitor visitor, void* context) {
        for (const auto& [binding, elements] : resource.bindings)
            visit_elements(elements, visitor, context);
    }
    void freeze_descriptor_dependencies(DescriptorOwnerContext& context, std::span<impl::GraphicResourceImpl* const> resources) {
        if (context.policy == DependencyPolicy::FIXED)
            return;
        if (!context.dependency_storage)
            throw std::logic_error("Descriptor owner cannot freeze dependencies");
        context.node.begin_update();
        struct Guard {
            DependencyNode& node;
            ~Guard() { node.end_update(); }
        } guard{context.node};
        for (auto* resource : resources) {
            if (!resource || resource->owner_context != &context)
                throw std::invalid_argument("Descriptor freeze requires one ownership root");
            require_descriptor_valid(*resource);
            if (resource->recording)
                throw std::logic_error("Descriptor resource is being recorded");
            resource->use.require_idle();
            require_descriptor_valid(*resource);
        }
        auto prepared = *context.dependency_storage;
        for (const auto* resource : resources)
            visit_descriptor_edges(*resource, [](void* output, const OwnerRef& owner) { detail::retain_owner(*static_cast<std::vector<OwnerRef>*>(output), owner); }, &prepared);
        context.node.check_additions(prepared);
        context.dependency_storage->swap(prepared);
        context.dependencies = *context.dependency_storage;
        context.policy = DependencyPolicy::FIXED;
        // 全ownerを固定親へ移した後にだけ内部参照へ正規化する。
        for (auto* resource : resources)
            for (auto& [binding, elements] : resource->bindings)
                for (auto& element : elements) {
                    context.normalize(element.owners);
                    context.normalize(element.constraint.owners);
                }
    }
    impl::GraphicResourceImpl::BindingConstraint descriptor_constraint(const ResourceUse& use, const DescriptorOwnerContext* context) {
        impl::GraphicResourceImpl::BindingConstraint result;
        for (const auto& state : {use.state, use.constraint_state}) {
            if (!state && !state.use_count())
                continue;
            if (!state.use_count())
                throw std::invalid_argument("Descriptor logical state requires an owning reference");
            detail::retain_owner(result.owners, state);
            if (state && !std::ranges::contains(result.states, state.get()))
                result.states.push_back(state.get());
        }
        detail::retain_owner(result.owners, use.owner);
        detail::retain_owner(result.owners, use.constraint_owner);
        if (context)
            context->normalize(result.owners);
        return result;
    }
    void require_descriptor_valid(const impl::GraphicResourceImpl& resource) {
        if (resource.owner_context && resource.owner_context->node.retiring())
            throw std::logic_error("Descriptor ownership root has been retired");
        if (!resource.pool || !resource.pool->descriptor_pool || !resource.descriptor_set || resource.pool->retiring)
            throw std::logic_error("Descriptor resource has been destroyed");
    }
    void require_descriptor_idle(const impl::GraphicResourceImpl& resource) {
        require_descriptor_valid(resource);
        if (resource.owner_context && resource.owner_context->node.updating())
            throw std::logic_error("Descriptor ownership root is being updated");
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
            if (!point.descriptorCount)
                continue;
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
            if (std::ranges::any_of(elements[i].owners, [](const auto& owner) { return !owner.use_count(); }))
                throw std::invalid_argument("Descriptor physical resource requires an owning reference");
            if (resource.owner_context) {
                resource.owner_context->normalize(elements[i].owners);
                resource.owner_context->normalize(elements[i].constraint.owners);
            }
            elements[i].initialized = true;
            prepared[write.dstArrayElement + i] = std::move(elements[i]);
        }
        DescriptorMutation mutation(resource);
        mutation.check(prepared);
        // cycle検査と確保をnative前に済ませ、公開後に旧ownerを解放する。
        auto& target = resource.bindings.try_emplace(write.dstBinding).first->second;
        write.dstSet = resource.descriptor_set;
        write.descriptorCount = static_cast<std::uint32_t>(elements.size());
        operation::write_descriptor(resource.native_device, write);
        target.swap(prepared);
        mutation.finish();
    }
    void replace_descriptor_constraints(impl::GraphicResourceImpl& resource, size_t binding, std::span<const ResourceUse> uses) {
        require_descriptor_idle(resource);
        if (!resource.layout)
            throw std::logic_error("Descriptor layout metadata is unavailable");
        const auto& point = resource.layout->layout_bindings.at(binding);
        if (!point.descriptorCount && uses.empty())
            return;
        auto& elements = resource.bindings.at(point.binding);
        if (uses.size() != elements.size())
            throw std::invalid_argument("Descriptor constraint array size mismatch");
        auto prepared = elements;
        for (size_t i = 0; i < uses.size(); ++i)
            prepared[i].constraint = descriptor_constraint(uses[i], resource.owner_context);
        DescriptorMutation mutation(resource);
        mutation.check(prepared);
        elements.swap(prepared);
        mutation.finish();
    }
} // namespace VSL_NAMESPACE::lifetime
