#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <algorithm>
#include <vector>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        bool merge_owner(OwnerRef& existing, OwnerRef& owner);
        bool merge_owner(OwnerRef& existing, OwnerRef& owner) {
            if (existing.owner_before(owner) || owner.owner_before(existing))
                return false;
            if (existing.managed() && owner.managed() && detail::OwnerAccess::Node(existing) != detail::OwnerAccess::Node(owner))
                throw std::invalid_argument("One owner cannot belong to different dependency nodes");
            if (!existing.managed() && owner.managed())
                existing = std::move(owner);
            return true;
        }
    } // namespace
    void DependencyNode::publish(const std::shared_ptr<void>& owner, const void* context, Edges edges) {
        if (_published || _retiring || !owner.use_count())
            throw std::invalid_argument("Dependency node requires one publication with a live owner");
        _owner = owner;
        _context = context;
        _edges = edges;
        _published = true;
    }
    void DependencyNode::retire() noexcept {
        _retiring = true;
        _edges = nullptr;
        _context = nullptr;
    }
    void DependencyNode::begin_update() {
        if (!_published || _retiring || _updating || _owner.expired())
            throw std::logic_error("Ownership root is unavailable for update");
        _updating = true;
    }
    void DependencyNode::visit_edges(EdgeVisitor visitor, void* context) const {
        if (!_retiring && _edges)
            _edges(_context, visitor, context);
    }
    void DependencyNode::check_additions(std::span<const OwnerRef> owners) const {
        check_additions(&owners, [](const void* context, EdgeVisitor visitor, void* output) {
            for (const auto& owner : *static_cast<const std::span<const OwnerRef>*>(context))
                visitor(output, owner);
        });
    }
    void DependencyNode::check_additions(const void* context, Edges edges) const {
        std::vector<const DependencyNode*> pending, visited;
        const auto append = [](void* context, const OwnerRef& owner) {
            if (auto* node = detail::OwnerAccess::Node(owner))
                static_cast<std::vector<const DependencyNode*>*>(context)->push_back(node);
        };
        edges(context, append, &pending);
        while (!pending.empty()) {
            const auto* target = pending.back();
            pending.pop_back();
            if (target == this)
                throw std::invalid_argument("Resource ownership would form a cycle");
            if (target->retiring() || target->updating())
                throw std::logic_error("Dependency root is being changed");
            if (std::ranges::contains(visited, target))
                continue;
            visited.push_back(target);
            target->visit_edges(append, &pending);
        }
    }
    OwnerRef DependencyNode::owner_ref() const {
        return ref(_owner.lock());
    }
    void detail::retain_owner(std::vector<OwnerRef>& owners, OwnerRef owner) {
        if (!owner.use_count())
            return;
        for (auto& existing : owners)
            if (merge_owner(existing, owner))
                return;
        owners.push_back(std::move(owner));
    }
    void detail::retain_owner(std::span<OwnerRef> slots, size_t& count, OwnerRef owner) {
        if (!owner.use_count())
            return;
        for (auto& existing : slots.first(count))
            if (merge_owner(existing, owner))
                return;
        if (count >= slots.size())
            throw std::length_error("Owner dependency capacity exceeded");
        slots[count++] = std::move(owner);
    }
} // namespace VSL_NAMESPACE::lifetime
