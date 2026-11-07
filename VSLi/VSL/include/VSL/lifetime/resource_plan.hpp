#pragma once
#include "resource_id.hpp"
#include "../storage/resource_layout.hpp"
#include <array>
#include <span>

namespace VSL_NAMESPACE::typed {
    static_assert(sizeof(LifetimeScope) == sizeof(std::uint32_t), "Resource plan lifetime ABI mismatch");
    struct ResourceRequest {
        ResourceId resource;
        storage::ResourceKind kind;
        bool external = false;
    };
    struct ResourcePlacement {
        ResourceId resource;
        storage::ResourceKind kind;
        bool external;
        size_t slot, offset;
        static constexpr size_t NO_SLOT = std::numeric_limits<size_t>::max();
    };
    // capacityは宣言数の上限。runtimeの実数はrequests.size()、payload容量とは独立する。
    template<size_t capacity>
    class ResourcePlan {
        std::array<ResourcePlacement, capacity> _placements{};
        size_t _count = 0, _slots = 0, _bytes = 0, _alignment = 1;
        LifetimeGroup _lifetime;

    public:
        constexpr explicit ResourcePlan(std::span<const ResourceRequest> requests, LifetimeGroup lifetime = {LifetimeScope::SHARED, 0});
        constexpr size_t size() const { return _count; }
        constexpr size_t slots() const { return _slots; }
        constexpr size_t bytes() const { return _bytes; }
        constexpr size_t alignment() const { return _alignment; }
        constexpr LifetimeGroup lifetime() const { return _lifetime; }
        constexpr std::span<const ResourcePlacement> placements() const& { return {_placements.data(), _count}; }
        constexpr std::span<const ResourcePlacement> placements() const&& = delete;
        constexpr ResourcePlacement placement(ResourceId resource) const;
        constexpr void require_region(size_t bytes, size_t alignment) const;
    };

    template<size_t capacity>
    constexpr ResourcePlan<capacity>::ResourcePlan(std::span<const ResourceRequest> requests, LifetimeGroup lifetime) : _lifetime(lifetime) {
        if (requests.size() > capacity)
            throw std::length_error("Resource plan capacity exceeded");
        if ((lifetime.scope != LifetimeScope::SHARED && lifetime.scope != LifetimeScope::FRAME && lifetime.scope != LifetimeScope::TRANSIENT) ||
            (lifetime.scope == LifetimeScope::SHARED && lifetime.instance))
            throw std::invalid_argument("Invalid resource lifetime group");
        for (const auto& request : requests) {
            const auto layout = storage::resource_slot_layout(request.kind);
            for (size_t i = 0; i < _count; ++i)
                if (_placements[i].resource == request.resource)
                    throw std::invalid_argument("Duplicate resource ID in placement plan");
            auto result = ResourcePlacement{request.resource, request.kind, request.external, ResourcePlacement::NO_SLOT, ResourcePlacement::NO_SLOT};
            if (!request.external) {
                result.slot = _slots++;
                result.offset = storage::checked_align(_bytes, layout.alignment);
                _bytes = storage::checked_add(result.offset, layout.size);
                if (_alignment < layout.alignment)
                    _alignment = layout.alignment;
            }
            _placements[_count++] = result;
        }
        _bytes = storage::checked_align(_bytes, _alignment);
        if (!std::is_constant_evaluated())
            storage::detail::require_resource_layout(std::integral_constant<std::uint64_t, storage::RESOURCE_LAYOUT_ABI>{});
    }
    template<size_t capacity>
    constexpr ResourcePlacement ResourcePlan<capacity>::placement(ResourceId resource) const {
        for (size_t i = 0; i < _count; ++i)
            if (_placements[i].resource == resource)
                return _placements[i];
        throw std::out_of_range("Unknown resource ID in placement plan");
    }
    template<size_t capacity>
    constexpr void ResourcePlan<capacity>::require_region(size_t bytes, size_t alignment) const {
        if (!std::is_constant_evaluated())
            storage::detail::require_resource_layout(std::integral_constant<std::uint64_t, storage::RESOURCE_LAYOUT_ABI>{});
        (void)storage::checked_align(0, alignment);
        if (bytes < _bytes || alignment < _alignment)
            throw std::invalid_argument("CPU region does not satisfy the resource plan");
    }
} // namespace VSL_NAMESPACE::typed
