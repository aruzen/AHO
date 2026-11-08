#pragma once
#include "owner_ref.hpp"
#include <span>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    enum class DependencyPolicy {
        FIXED,
        DYNAMIC
    };
    // owner内に正規nodeを一つ埋め込み、公開後は移動しない。
    // visitorは実際の強所有を漏れなく副作用なしで列挙する。
    // destructorの先頭でretireし、関連rootの更新/破棄は呼出側で直列化する。
    class DependencyNode {
        std::weak_ptr<void> _owner;
        const void* _context = nullptr;
        bool _published = false, _retiring = false, _updating = false;

    public:
        using EdgeVisitor = void (*)(void*, const OwnerRef&);
        using Edges = void (*)(const void*, EdgeVisitor, void*);

    private:
        Edges _edges = nullptr;

    public:
        DependencyNode() = default;
        DependencyNode(const DependencyNode&) = delete;
        DependencyNode& operator=(const DependencyNode&) = delete;
        ~DependencyNode() { retire(); }
        void publish(const std::shared_ptr<void>& owner, const void* context = nullptr, Edges edges = nullptr);
        void retire() noexcept;
        bool retiring() const noexcept { return _retiring; }
        bool updating() const noexcept { return _updating; }
        void begin_update();
        void end_update() noexcept { _updating = false; }
        void visit_edges(EdgeVisitor visitor, void* context) const;
        void check_additions(std::span<const OwnerRef> owners) const;
        void check_additions(const void* context, Edges edges) const;
        template<typename T>
        bool matches(const std::shared_ptr<T>& owner) const noexcept;
        template<typename T>
        OwnedRef<T> ref(std::shared_ptr<T> owner) const;
        OwnerRef owner_ref() const;
    };
    template<typename T>
    bool DependencyNode::matches(const std::shared_ptr<T>& owner) const noexcept {
        return _published && owner.use_count() && !_owner.owner_before(owner) && !owner.owner_before(_owner);
    }
    template<typename T>
    OwnedRef<T> DependencyNode::ref(std::shared_ptr<T> owner) const {
        if (_retiring || !matches(owner))
            throw std::invalid_argument("Dependency node requires its canonical owner");
        return {std::move(owner), this};
    }
} // namespace VSL_NAMESPACE::lifetime
