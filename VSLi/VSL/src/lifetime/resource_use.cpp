#include <VSL/lifetime/resource_use.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    ResourceUse ResourceUse::ForBuffer(const std::shared_ptr<impl::BufferImpl>& buffer, ResourceUse constraint) {
        if (!buffer || !buffer->use)
            throw std::invalid_argument("Buffer requires a constructed storage generation");
        return ResourceUse{{buffer, buffer->use}, buffer}.with_constraint(std::move(constraint));
    }
    ResourceUse ResourceUse::with_constraint(ResourceUse constraint) const {
        if (constraint_state || constraint_owner.use_count()) {
            if (constraint.state || constraint.owner.use_count() || constraint.constraint_state || constraint.constraint_owner.use_count())
                throw std::invalid_argument("Resource already has a logical constraint");
            return *this;
        }
        auto use = *this;
        if (constraint.state == use.state) {
            use.constraint_state = std::move(constraint.constraint_state);
            use.constraint_owner = std::move(constraint.constraint_owner);
        } else {
            if (constraint.constraint_state || constraint.constraint_owner.use_count())
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
                buffer.retain_until_complete(owner);
            else
                completion.retain(owner);
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
        if (!resource)
            throw std::invalid_argument("Descriptor use requires a resource");
        auto group = std::make_shared<ResourceUseGroup>(bindings, resource);
        group->_resource = resource.get();
        group->_retention = retention;
        return group;
    }
    void ResourceUseGroup::require_idle() const {
        _state.require_idle();
    }
    void ResourceUseGroup::replace(size_t binding, std::vector<ResourceUse> resources) {
        require_idle();
        _bindings.at(binding) = std::move(resources);
    }
    void ResourceUseGroup::record(const CommandBuffer& buffer) const {
        const auto completion = buffer.completion();
        if (!completion || completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Tracked descriptor sets require a recording phase");
        if (_owner.use_count()) {
            if (_retention == ResourceRetention::COMPLETION)
                buffer.retain_until_complete(_owner);
            else
                completion.retain(_owner);
        }
        _state.mark_used(completion);
        if (_resource)
            lifetime::record_descriptor_buffers(*_resource, buffer, _retention);
        for (const auto& binding : _bindings)
            for (const auto& resource : binding)
                resource.record(buffer, _retention);
    }
} // namespace VSL_NAMESPACE
