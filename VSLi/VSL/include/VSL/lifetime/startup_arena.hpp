#pragma once
#include "../storage/startup_plan.hpp"
#include "../storage/arena_memory.hpp"
#include "../vulkan/device.hpp"
#include <variant>

namespace VSL_NAMESPACE::lifetime {
    namespace detail {
        struct StartupArenaOwner;
    }
    using InstanceSource = std::variant<typed::ResourceId, VulkanAccessor>;
    using PhysicalDeviceSource = std::variant<typed::ResourceId, PhysicalDevice>;
    using SurfaceSource = std::variant<typed::ResourceId, Surface>;
    struct PhysicalDeviceInit {
        InstanceSource instance;
        // 省略時は既存VSLの採点規則で選ぶ。候補配列は生成中だけのscratch。
        std::optional<size_t> index;
    };
    struct SurfaceInit {
        InstanceSource instance;
        OSWindowOwner window;
    };
    struct DeviceInit {
        PhysicalDeviceSource physical;
        DeviceCreateInfo info;
    };
    struct GraphicsDeviceInit {
        PhysicalDeviceSource physical;
        // 直接渡すSurfaceは生成中だけ保持する。永続依存へ追加しない。
        SurfaceSource surface;
        bool validation = VALIDATION;
        PresentFencePolicy present_fences = PresentFencePolicy::LEGACY;
    };
    using StartupInit = std::variant<InstanceCreateInfo, PhysicalDeviceInit, SurfaceInit, DeviceInit, GraphicsDeviceInit,
                                     VulkanAccessor, PhysicalDevice, Surface, LogicalDeviceAccessor>;
    class StartupArena {
        std::shared_ptr<detail::StartupArenaOwner> _owner;
        StartupArena(std::span<const storage::StartupPlacement> placements, std::span<const StartupInit> initializers,
                     std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment);

    public:
        template<size_t capacity>
        StartupArena(const storage::StartupPlan<capacity>& plan, std::span<const StartupInit> initializers);
        template<size_t capacity>
        StartupArena(const storage::StartupPlan<capacity>& plan, std::span<const StartupInit> initializers, storage::ArenaMemory memory);
        OwnerRef owner_ref() const;
        size_t size() const;
        size_t slots() const;
        VulkanAccessor instance(typed::ResourceId id) const;
        PhysicalDevice physical_device(typed::ResourceId id) const;
        Surface surface(typed::ResourceId id) const;
        LogicalDeviceAccessor device(typed::ResourceId id) const;
    };

    template<size_t capacity>
    StartupArena::StartupArena(const storage::StartupPlan<capacity>& plan, std::span<const StartupInit> initializers)
        : StartupArena(plan, initializers, storage::ArenaMemory(plan.bytes(), plan.alignment())) {}
    template<size_t capacity>
    StartupArena::StartupArena(const storage::StartupPlan<capacity>& plan, std::span<const StartupInit> initializers, storage::ArenaMemory memory)
        : StartupArena((plan.require_region(memory.size(), memory.alignment()), plan.placements()), initializers,
                       memory._region, plan.bytes(), plan.alignment()) {}
} // namespace VSL_NAMESPACE::lifetime
