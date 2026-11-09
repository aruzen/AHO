#pragma once
#include <VSL/lifetime/owner_access.hpp>
#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace vsl_test {
    struct FixedParentOwner {
        vsl::lifetime::DependencyNode node;
        vsl::lifetime::OwnerRef parent;
        explicit FixedParentOwner(vsl::lifetime::OwnerRef parent) : parent(std::move(parent)) {}
        ~FixedParentOwner() { node.retire(); }
        static std::shared_ptr<FixedParentOwner> Create(vsl::lifetime::OwnerRef parent) {
            auto owner = std::make_shared<FixedParentOwner>(std::move(parent));
            owner->node.publish(owner, owner.get(), Visit);
            return owner;
        }
        static void Visit(const void* context, vsl::lifetime::DependencyNode::EdgeVisitor visitor, void* output) {
            visitor(output, static_cast<const FixedParentOwner*>(context)->parent);
        }
    };
    template<typename Impl>
    vsl::lifetime::OwnerRef canonical_owner(const vsl::lifetime::OwnedRef<Impl>& value) {
        if (!value.managed())
            throw std::invalid_argument("Test resource has no canonical node");
        return value;
    }
    template<typename Impl>
    vsl::lifetime::OwnerRef canonical_owner(const std::shared_ptr<Impl>& value) {
        if (!value || !value->owner_node)
            throw std::invalid_argument("Test resource has no canonical node");
        return value->owner_node->ref(value);
    }
    inline void expect_fixed_parent(const vsl::lifetime::OwnerRef& child, const vsl::lifetime::OwnerRef& parent, bool direct = true) {
        using Access = vsl::lifetime::detail::OwnerAccess;
        ASSERT_TRUE(child.managed());
        ASSERT_TRUE(parent.managed());
        if (direct) {
            std::vector<vsl::lifetime::OwnerRef> edges;
            Access::Node(child)->visit_edges([](void* context, const auto& edge) {
                static_cast<std::vector<vsl::lifetime::OwnerRef>*>(context)->push_back(edge);
            },
                                             &edges);
            size_t matches = 0;
            for (const auto& edge : edges)
                if (!edge.owner_before(parent) && !parent.owner_before(edge)) {
                    EXPECT_TRUE(edge.managed());
                    ++matches;
                }
            EXPECT_EQ(matches, 1u);
        }
        const std::array additions{child};
        EXPECT_THROW(Access::Node(parent)->check_additions(additions), std::invalid_argument);
    }
} // namespace vsl_test
