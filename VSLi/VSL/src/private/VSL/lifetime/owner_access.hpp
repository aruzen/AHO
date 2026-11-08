#pragma once
#include <VSL/lifetime/dependency_node.hpp>
#include <vector>

namespace VSL_NAMESPACE::lifetime::detail {
    struct OwnerAccess {
        template<typename T>
        static const DependencyNode* Node(const OwnedRef<T>& owner) noexcept { return owner.use_count() ? owner._node : nullptr; }
        template<typename T>
        static const std::shared_ptr<T>& Shared(const OwnedRef<T>& owner) noexcept { return owner._value; }
    };
    void retain_owner(std::vector<OwnerRef>& owners, OwnerRef owner);
    void retain_owner(std::span<OwnerRef> slots, size_t& count, OwnerRef owner);
    template<typename T>
    OwnedRef<T> owned_ref(std::shared_ptr<T> value, const DependencyNode* node) {
        if (node && node->matches(value))
            return node->ref(std::move(value));
        return value;
    }
    template<typename Owner, typename Impl>
    std::shared_ptr<Impl> publish_leaf(std::shared_ptr<Owner> owner, Impl* value) {
        owner->node.publish(owner);
        value->owner_node = &owner->node;
        return {std::move(owner), value};
    }
    template<typename Owner, typename Impl>
    std::shared_ptr<Impl> publish_device_leaf(std::shared_ptr<Owner> owner, Impl* value) {
        owner->node.publish(owner, owner.get(), [](const void* input, DependencyNode::EdgeVisitor visitor, void* context) {
            visitor(context, static_cast<const Owner*>(input)->device.owner_ref());
        });
        value->owner_node = &owner->node;
        return {std::move(owner), value};
    }
} // namespace VSL_NAMESPACE::lifetime::detail
