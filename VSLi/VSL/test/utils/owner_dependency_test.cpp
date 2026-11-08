#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/resource_use.hpp>
#include <gtest/gtest.h>
#include <functional>

namespace {
    namespace l = vsl::lifetime;
    struct OwnerRoot {
        l::DependencyNode node;
        std::vector<l::OwnerRef> dependencies;
        std::function<void()> released;
        int value = 0;
        vsl::ResourceUseState state;
        ~OwnerRoot();
        static void Visit(const void* input, l::DependencyNode::EdgeVisitor visitor, void* context);
    };
    std::shared_ptr<OwnerRoot> make_root();
    void connect(const std::shared_ptr<OwnerRoot>& root, std::vector<l::OwnerRef> owners);
    OwnerRoot::~OwnerRoot() {
        node.retire();
        if (released)
            released();
    }
    void OwnerRoot::Visit(const void* input, l::DependencyNode::EdgeVisitor visitor, void* context) {
        for (const auto& owner : static_cast<const OwnerRoot*>(input)->dependencies)
            visitor(context, owner);
    }
    std::shared_ptr<OwnerRoot> make_root() {
        auto root = std::make_shared<OwnerRoot>();
        root->node.publish(root, root.get(), OwnerRoot::Visit);
        return root;
    }
    void connect(const std::shared_ptr<OwnerRoot>& root, std::vector<l::OwnerRef> owners) {
        root->node.begin_update();
        try {
            root->node.check_additions(owners);
            root->dependencies.swap(owners);
        } catch (...) {
            root->node.end_update();
            throw;
        }
        root->node.end_update();
    }
    TEST(OwnerDependencyTest, AliasesMovesAndExplicitLegacyEscapePreserveTheOwnerContract) {
        auto root = make_root();
        auto value = root->node.ref(root).alias(&root->value);
        l::OwnerRef erased = value;
        auto null_alias = erased.alias(static_cast<void*>(nullptr));
        EXPECT_TRUE(value.managed());
        EXPECT_EQ(value, erased.alias(&root->value));
        EXPECT_FALSE(null_alias);
        EXPECT_GT(null_alias.use_count(), 0);
        EXPECT_TRUE(null_alias.managed());
        auto moved = std::move(value);
        EXPECT_FALSE(value.managed());
        EXPECT_EQ(value.use_count(), 0);
        EXPECT_TRUE(moved.managed());
        l::OwnedRef<int> opaque = moved.share();
        EXPECT_FALSE(opaque.managed());
        EXPECT_EQ(opaque, moved);
        EXPECT_FALSE(moved.owner_before(opaque));
        EXPECT_FALSE(opaque.owner_before(moved));
    }
    TEST(OwnerDependencyTest, AssignmentAndResetPublishBothPartsBeforeTheOldOwnerCallback) {
        auto old = make_root(), next = make_root();
        auto value = old->node.ref(old).alias(&old->value);
        size_t calls = 0;
        old->released = [&] {
            ++calls;
            EXPECT_EQ(value.get(), &next->value);
            EXPECT_TRUE(value.managed());
            EXPECT_EQ(l::detail::OwnerAccess::Node(value), &next->node);
        };
        old.reset();
        value = next->node.ref(next).alias(&next->value);
        EXPECT_EQ(calls, 1u);
        next->released = [&] {
            ++calls;
            EXPECT_FALSE(value.managed());
            EXPECT_EQ(value.use_count(), 0);
            EXPECT_EQ(l::detail::OwnerAccess::Node(value), nullptr);
        };
        next.reset();
        value.reset();
        EXPECT_EQ(calls, 2u);
    }
    TEST(OwnerDependencyTest, CanonicalControlIdentityRejectsAnotherRootAndDuplicateKnownNodes) {
        auto first = make_root(), second = make_root();
        EXPECT_THROW(first->node.ref(second), std::invalid_argument);
        EXPECT_THROW(first->node.publish(first), std::invalid_argument);
        l::DependencyNode duplicate;
        duplicate.publish(first);
        std::vector<l::OwnerRef> owners{first->node.owner_ref()};
        EXPECT_THROW(l::detail::retain_owner(owners, duplicate.owner_ref()), std::invalid_argument);
        EXPECT_EQ(owners.size(), 1u);
        EXPECT_EQ(l::detail::OwnerAccess::Node(owners.front()), &first->node);
    }
    TEST(OwnerDependencyTest, DedupUpgradesOpaqueAliasesWithoutLosingKnownEdgesOrNullOwners) {
        auto source = make_root(), target = make_root();
        connect(target, {source->node.owner_ref()});
        std::vector<l::OwnerRef> owners{target};
        EXPECT_FALSE(owners.front().managed());
        l::detail::retain_owner(owners, target->node.owner_ref().alias(static_cast<void*>(nullptr)));
        ASSERT_EQ(owners.size(), 1u);
        EXPECT_TRUE(owners.front().managed());
        EXPECT_GT(owners.front().use_count(), 0);
        EXPECT_THROW(connect(source, owners), std::invalid_argument);
        EXPECT_TRUE(source->dependencies.empty());
        EXPECT_EQ(target->dependencies.size(), 1u);
    }
    TEST(OwnerDependencyTest, CyclesInBothDirectionsAreRejectedAndRemovingAnEdgeAllowsTheUpdate) {
        auto first = make_root(), second = make_root(), third = make_root();
        connect(first, {second->node.owner_ref()});
        connect(second, {third->node.owner_ref()});
        EXPECT_THROW(connect(third, {first->node.owner_ref()}), std::invalid_argument);
        EXPECT_THROW(connect(first, {first->node.owner_ref()}), std::invalid_argument);
        EXPECT_TRUE(third->dependencies.empty());
        connect(first, {});
        EXPECT_NO_THROW(connect(third, {first->node.owner_ref()}));
        EXPECT_THROW(connect(first, {second->node.owner_ref()}), std::invalid_argument);
        std::weak_ptr<OwnerRoot> a = first, b = second, c = third;
        first.reset();
        second.reset();
        third.reset();
        EXPECT_TRUE(a.expired());
        EXPECT_TRUE(b.expired());
        EXPECT_TRUE(c.expired());
    }
    TEST(OwnerDependencyTest, PendingMutationAndRetirementRejectReentrantIncomingEdges) {
        auto first = make_root(), second = make_root();
        first->node.begin_update();
        EXPECT_THROW(first->node.begin_update(), std::logic_error);
        EXPECT_THROW(connect(second, {first->node.owner_ref()}), std::logic_error);
        first->node.end_update();
        EXPECT_NO_THROW(connect(second, {first->node.owner_ref()}));
        second->node.retire();
        EXPECT_THROW(second->node.owner_ref(), std::invalid_argument);
        EXPECT_THROW(second->node.begin_update(), std::logic_error);
        size_t count = 0;
        second->node.visit_edges([](void* value, const l::OwnerRef&) { ++*static_cast<size_t*>(value); }, &count);
        EXPECT_EQ(count, 0u);
    }
    TEST(OwnerDependencyTest, SameStateIdentityKeepsADifferentOwnerWhileFlatteningMatchingPhysicalUses) {
        auto a = make_root(), b = make_root();
        const vsl::ResourceUse physical{a->node.ref(a).alias(&a->state), a->node.owner_ref()};
        const vsl::ResourceUse logical{b->node.ref(b).alias(&b->state), b->node.owner_ref()};
        const auto nested = physical.with_constraint(logical);
        const auto flattened = physical.with_constraint(nested);
        EXPECT_EQ(flattened.state, physical.state);
        EXPECT_EQ(flattened.constraint_state, logical.state);
        EXPECT_FALSE(flattened.constraint_owner.owner_before(logical.owner));
        EXPECT_FALSE(logical.owner.owner_before(flattened.constraint_owner));
        const auto different_owner = physical.with_constraint({physical.state, b->node.owner_ref()});
        EXPECT_EQ(different_owner.constraint_state, physical.state);
        EXPECT_EQ(l::detail::OwnerAccess::Node(different_owner.constraint_owner), &b->node);
        const auto null_owner = b->node.owner_ref().alias(static_cast<void*>(nullptr));
        const auto null_constraint = physical.with_constraint({physical.state, null_owner});
        EXPECT_GT(null_constraint.constraint_owner.use_count(), 0);
        EXPECT_EQ(l::detail::OwnerAccess::Node(null_constraint.constraint_owner), &b->node);
        const auto null_state = b->node.ref(b).alias(static_cast<vsl::ResourceUseState*>(nullptr));
        const auto state_constraint = physical.with_constraint({null_state, {}});
        EXPECT_FALSE(state_constraint.constraint_state);
        EXPECT_GT(state_constraint.constraint_state.use_count(), 0);
        EXPECT_EQ(l::detail::OwnerAccess::Node(state_constraint.constraint_state), &b->node);
        EXPECT_THROW(state_constraint.with_constraint(logical), std::invalid_argument);
        const auto flattened_null = physical.with_constraint(state_constraint);
        EXPECT_EQ(l::detail::OwnerAccess::Node(flattened_null.constraint_state), &b->node);
        auto too_deep = nested;
        too_deep.owner = b->node.owner_ref();
        EXPECT_THROW(physical.with_constraint(too_deep), std::invalid_argument);
        const auto owner_only = vsl::ResourceUse{}.with_constraint({{}, b->node.owner_ref()});
        EXPECT_EQ(l::detail::OwnerAccess::Node(owner_only.constraint_owner), &b->node);
    }
} // namespace
