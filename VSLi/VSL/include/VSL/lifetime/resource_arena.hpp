#pragma once
#include "resource_plan.hpp"
#include "buffer_view.hpp"
#include "logical_use_state.hpp"
#include "../operation/image_create_info.hpp"
#include "../operation/sampler_create_info.hpp"
#include "../operation/descriptor_create_info.hpp"
#include "../storage/arena_memory.hpp"
#include "../vulkan/image.hpp"
#include "../vulkan/sampler.hpp"
#include <variant>

namespace VSL_NAMESPACE::lifetime {
    namespace detail {
        struct ResourceArenaOwner;
    }
    struct DescriptorSetInit {
        typed::ResourceId pool, layout;
    };
    using ResourceInit = std::variant<BufferCreateInfo, ImageCreateInfo, SamplerCreateInfo, BufferView, ImageAccessor, SamplerAccessor,
                                      DescriptorLayoutCreateInfo, DescriptorPoolCreateInfo, DescriptorSetInit,
                                      graphic_resource::BindingLayout, graphic_resource::Pool>;
    class ResourceArena {
        std::shared_ptr<detail::ResourceArenaOwner> _owner;
        ResourceArena(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                      std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                      std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment,
                      std::span<const LogicalUseState> dependencies);

    public:
        template<size_t capacity>
        ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers);
        template<size_t capacity>
        ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, storage::ArenaMemory memory,
                      std::span<const LogicalUseState> dependencies = {});
        std::shared_ptr<void> owner() const;
        size_t size() const;
        size_t slots() const;
        typed::LifetimeGroup lifetime() const;
        BufferView buffer(typed::ResourceId id) const;
        ImageAccessor image(typed::ResourceId id) const;
        SamplerAccessor sampler(typed::ResourceId id) const;
        graphic_resource::BindingLayout descriptor_layout(typed::ResourceId id) const;
        graphic_resource::Pool descriptor_pool(typed::ResourceId id) const;
        GraphicResource descriptor_set(typed::ResourceId id) const;
        // 非所有の物理Buffer全体。外部の部分viewに対してはbuffer()を使用する。
        BufferRef borrow_buffer(typed::ResourceId id) const&;
        BufferRef borrow_buffer(typed::ResourceId id) const&& = delete;
    };

    template<size_t capacity>
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers)
        : ResourceArena(device, plan, initializers, storage::ArenaMemory(plan.bytes(), plan.alignment())) {}
    template<size_t capacity>
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, storage::ArenaMemory memory,
                                 std::span<const LogicalUseState> dependencies)
        : ResourceArena((plan.require_region(memory.size(), memory.alignment()), device), plan.lifetime(), plan.placements(), initializers, memory._region, plan.bytes(), plan.alignment(), dependencies) {}
} // namespace VSL_NAMESPACE::lifetime
