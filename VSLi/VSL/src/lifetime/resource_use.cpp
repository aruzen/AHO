#include <VSL/lifetime/resource_use.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    ResourceUse ResourceUse::ForBuffer(const std::shared_ptr<impl::BufferImpl>& buffer, ResourceUse constraint) {
        if (!buffer || !buffer->use)
            throw std::invalid_argument("Buffer requires a constructed storage generation");
        const auto owner = lifetime::detail::owned_ref(buffer, buffer->owner_node);
        return ResourceUse{owner.alias(buffer->use), owner}.with_constraint(std::move(constraint));
    }
    ResourceUse ResourceUse::ForImage(const std::shared_ptr<impl::ImageImpl>& image) {
        if (!image || !image->use)
            throw std::invalid_argument("Image requires a constructed storage generation");
        const auto owner = lifetime::detail::owned_ref(image, image->owner_node);
        return {owner.alias(image->use), owner};
    }
    ResourceUse ResourceUse::ForSampler(const std::shared_ptr<impl::SamplerImpl>& sampler) {
        if (!sampler || !sampler->use)
            throw std::invalid_argument("Sampler requires a constructed storage generation");
        const auto owner = lifetime::detail::owned_ref(sampler, sampler->owner_node);
        return {owner.alias(sampler->use), owner};
    }
    ResourceUse ResourceUse::with_constraint(ResourceUse constraint) const {
        const auto present = [](const auto& state) { return bool(state) || state.use_count(); };
        if (present(constraint_state) || constraint_owner.use_count()) {
            if (present(constraint.state) || constraint.owner.use_count() || present(constraint.constraint_state) || constraint.constraint_owner.use_count())
                throw std::invalid_argument("Resource already has a logical constraint");
            return *this;
        }
        auto use = *this;
        const auto same_owner = [](const auto& first, const auto& second) { return !first.owner_before(second) && !second.owner_before(first); };
        if (constraint.state == use.state && same_owner(constraint.state, use.state) &&
            (!constraint.owner.use_count() || same_owner(constraint.owner, use.owner))) {
            use.constraint_state = std::move(constraint.constraint_state);
            use.constraint_owner = std::move(constraint.constraint_owner);
        } else {
            if (present(constraint.constraint_state) || constraint.constraint_owner.use_count())
                throw std::invalid_argument("Cannot attach a different storage generation as a logical constraint");
            use.constraint_state = std::move(constraint.state);
            use.constraint_owner = std::move(constraint.owner);
        }
        return use;
    }
    void ResourceUse::record(const CommandBuffer& buffer, ResourceRetention retention) const {
        if (!state && !owner.use_count() && !constraint_state && !constraint_owner.use_count())
            return;
        const auto completion = buffer.completion();
        if (!completion || completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Tracked resources require a recording phase");
        const auto retain = [&](const auto& owner) {
            if (!owner.use_count())
                return;
            if (retention == ResourceRetention::COMPLETION)
                buffer.retain_until_complete(lifetime::detail::OwnerAccess::Shared(owner));
            else
                completion.retain(lifetime::detail::OwnerAccess::Shared(owner));
        };
        retain(owner);
        retain(constraint_owner);
        if (state)
            state->mark_used(completion);
        if (constraint_state)
            constraint_state->mark_used(completion);
    }
    ResourceUseGroup::ResourceUseGroup(size_t bindings, std::shared_ptr<void> owner)
        : _owner(std::move(owner)), _bindings(bindings) {}
    std::shared_ptr<ResourceUseGroup> ResourceUseGroup::ForDescriptor(size_t bindings, const std::shared_ptr<impl::GraphicResourceImpl>& resource, ResourceRetention retention) {
        if (!resource || !resource.use_count())
            throw std::invalid_argument("Descriptor use requires a resource");
        if (!resource->layout || resource->layout->layout_bindings.size() != bindings)
            throw std::invalid_argument("Descriptor use binding count mismatch");
        auto group = std::make_shared<ResourceUseGroup>(0, resource);
        group->_resource = resource.get();
        group->_retention = retention;
        return group;
    }
    void ResourceUseGroup::require_idle() const {
        const auto owner = _owner;
        auto* const resource = _resource;
        if (resource) {
            lifetime::require_descriptor_idle(*resource);
        } else
            _state.require_idle();
    }
    void ResourceUseGroup::replace(size_t binding, std::vector<ResourceUse> resources) {
        const auto owner = _owner;
        auto* const resource = _resource;
        if (resource) {
            lifetime::replace_descriptor_constraints(*resource, binding, resources);
            return;
        }
        _state.require_idle();
        _bindings.at(binding) = std::move(resources);
    }
    void ResourceUseGroup::record(const CommandBuffer& buffer) const {
        const auto owner = _owner;
        auto* const resource = _resource;
        const auto retention = _retention;
        const auto completion = buffer.completion();
        if (!completion || completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Tracked descriptor sets require a recording phase");
        if (owner.use_count()) {
            if (retention == ResourceRetention::COMPLETION)
                buffer.retain_until_complete(owner);
            else
                completion.retain(owner);
        }
        if (resource) {
            lifetime::record_descriptor_resources(*resource, buffer, retention);
            return;
        }
        _state.mark_used(completion);
        for (const auto& binding : _bindings)
            for (const auto& resource : binding)
                resource.record(buffer, _retention);
    }
} // namespace VSL_NAMESPACE
