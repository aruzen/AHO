#pragma once
#include "resource_layout.hpp"
#include "../lifetime/resource_id.hpp"
#include <array>
#include <algorithm>
#include <span>

namespace VSL_NAMESPACE::storage {
    enum class StartupKind : std::uint32_t {
        INSTANCE,
        PHYSICAL_DEVICE,
        SURFACE,
        DEVICE
    };
    struct StartupRequest {
        typed::ResourceId resource;
        StartupKind kind;
        bool external = false;
    };
    struct StartupPlacement {
        typed::ResourceId resource;
        StartupKind kind;
        bool external;
        size_t slot, offset;
        static constexpr size_t NO_SLOT = std::numeric_limits<size_t>::max();
    };
    constexpr SlotLayout startup_slot_layout(StartupKind kind);
    // 一つのplanは一つの破棄単位。同時に起動する資源でも別寿命なら別planにする。
    template<size_t capacity>
    class StartupPlan {
        std::array<StartupPlacement, capacity> _placements{};
        size_t _count = 0, _slots = 0, _bytes = 0, _alignment = 1;

    public:
        constexpr explicit StartupPlan(std::span<const StartupRequest> requests);
        constexpr size_t size() const { return _count; }
        constexpr size_t slots() const { return _slots; }
        constexpr size_t bytes() const { return _bytes; }
        constexpr size_t alignment() const { return _alignment; }
        constexpr std::span<const StartupPlacement> placements() const& { return {_placements.data(), _count}; }
        constexpr std::span<const StartupPlacement> placements() const&& = delete;
        constexpr StartupPlacement placement(typed::ResourceId resource) const;
        constexpr void require_region(size_t bytes, size_t alignment) const;
    };

    constexpr SlotLayout startup_slot_layout(StartupKind kind) {
        switch (kind) {
            case StartupKind::INSTANCE:
                return {INSTANCE_SLOT_SIZE, INSTANCE_SLOT_ALIGNMENT, NO_SEPARATE_USE};
            case StartupKind::PHYSICAL_DEVICE:
                return {PHYSICAL_DEVICE_SLOT_SIZE, PHYSICAL_DEVICE_SLOT_ALIGNMENT, NO_SEPARATE_USE};
            case StartupKind::SURFACE:
                return {SURFACE_SLOT_SIZE, SURFACE_SLOT_ALIGNMENT, NO_SEPARATE_USE};
            case StartupKind::DEVICE:
                return {DEVICE_SLOT_SIZE, DEVICE_SLOT_ALIGNMENT, NO_SEPARATE_USE};
        }
        throw std::invalid_argument("Unknown startup implementation kind");
    }
    template<size_t capacity>
    constexpr StartupPlan<capacity>::StartupPlan(std::span<const StartupRequest> requests) {
        if (requests.size() > capacity)
            throw std::length_error("Startup plan capacity exceeded");
        for (const auto& request : requests) {
            const auto layout = startup_slot_layout(request.kind);
            for (size_t i = 0; i < _count; ++i)
                if (_placements[i].resource == request.resource)
                    throw std::invalid_argument("Duplicate startup resource ID");
            auto placement = StartupPlacement{request.resource, request.kind, request.external, StartupPlacement::NO_SLOT, StartupPlacement::NO_SLOT};
            if (!request.external) {
                placement.slot = _slots++;
                placement.offset = checked_align(_bytes, layout.alignment);
                _bytes = checked_add(placement.offset, layout.size);
                _alignment = std::max(_alignment, layout.alignment);
            }
            _placements[_count++] = placement;
        }
        _bytes = checked_align(_bytes, _alignment);
        if (!std::is_constant_evaluated())
            detail::require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>{});
    }
    template<size_t capacity>
    constexpr StartupPlacement StartupPlan<capacity>::placement(typed::ResourceId resource) const {
        for (size_t i = 0; i < _count; ++i)
            if (_placements[i].resource == resource)
                return _placements[i];
        throw std::out_of_range("Unknown startup resource ID");
    }
    template<size_t capacity>
    constexpr void StartupPlan<capacity>::require_region(size_t bytes, size_t alignment) const {
        if (!std::is_constant_evaluated())
            detail::require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>{});
        (void)checked_align(0, alignment);
        if (bytes < _bytes || alignment < _alignment)
            throw std::invalid_argument("CPU region does not satisfy the startup plan");
    }
} // namespace VSL_NAMESPACE::storage
