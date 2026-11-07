#pragma once
#include "resource_plan.hpp"
#include "buffer_view.hpp"
#include "../operation/image_create_info.hpp"
#include "../operation/sampler_create_info.hpp"
#include "../storage/arena_memory.hpp"
#include "../vulkan/image.hpp"
#include "../vulkan/sampler.hpp"
#include <variant>

namespace VSL_NAMESPACE::lifetime {
    namespace detail {
        struct ResourceArenaOwner;
    }
    using ResourceInit = std::variant<BufferCreateInfo, ImageCreateInfo, SamplerCreateInfo, BufferView, ImageAccessor, SamplerAccessor>;
    class ResourceArena {
        std::shared_ptr<detail::ResourceArenaOwner> _owner;
        ResourceArena(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                      std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                      std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment);

    public:
        template<size_t capacity>
        ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers);
        template<size_t capacity>
        ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, storage::ArenaMemory memory);
        std::shared_ptr<void> owner() const;
        size_t size() const;
        size_t slots() const;
        typed::LifetimeGroup lifetime() const;
        BufferView buffer(typed::ResourceId id) const;
        ImageAccessor image(typed::ResourceId id) const;
        SamplerAccessor sampler(typed::ResourceId id) const;
        // 非所有の物理Buffer全体。外部の部分viewに対してはbuffer()を使用する。
        BufferRef borrow_buffer(typed::ResourceId id) const&;
        BufferRef borrow_buffer(typed::ResourceId id) const&& = delete;
    };

    template<size_t capacity>
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers)
        : ResourceArena(device, plan, initializers, storage::ArenaMemory(plan.bytes(), plan.alignment())) {}
    template<size_t capacity>
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, storage::ArenaMemory memory)
        : ResourceArena((plan.require_region(memory.size(), memory.alignment()), device), plan.lifetime(), plan.placements(), initializers, memory._region, plan.bytes(), plan.alignment()) {}
} // namespace VSL_NAMESPACE::lifetime
