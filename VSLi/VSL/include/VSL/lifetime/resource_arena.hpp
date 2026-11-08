#pragma once
#include "resource_plan.hpp"
#include "buffer_view.hpp"
#include "logical_use_state.hpp"
#include "dependency_node.hpp"
#include "../operation/image_create_info.hpp"
#include "../operation/sampler_create_info.hpp"
#include "../operation/descriptor_create_info.hpp"
#include "../operation/shader_create_info.hpp"
#include "../operation/pipeline_layout_create_info.hpp"
#include "../vulkan/pipeline_layout.hpp"
#include "../vulkan/render_pass.hpp"
#include "../vulkan/pipeline.hpp"
#include "../vulkan/frame_buffer.hpp"
#include "../vulkan/command.hpp"
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
    using PipelineLayoutSource = std::variant<typed::ResourceId, graphic_resource::BindingLayout>;
    struct PipelineLayoutInit {
        // indexはnative set番号。同じIDを複数回参照でき、holeも明示した空Layoutで埋める。
        // IDはarenaの明示資源。直接のLayoutはnative構築呼出し中だけ保持する。
        std::vector<PipelineLayoutSource> layouts;
        std::vector<PushConstantRange> push_ranges;
    };
    using RuntimeLayoutSource = std::variant<typed::ResourceId, PipelineLayoutAccessor>;
    using PipelinePassSource = std::variant<typed::ResourceId, RenderPassAccessor>;
    using PipelineShaderSource = std::variant<typed::ResourceId, ShaderAccessor>;
    struct GraphicsPipelineInit {
        RuntimeLayoutSource layout;
        PipelinePassSource pass;
        GraphicsPipelineState state;
        std::vector<PipelineShaderSource> shaders;
        std::uint32_t subpass = 0;
    };
    struct ComputePipelineInit {
        RuntimeLayoutSource layout;
        PipelineShaderSource shader;
    };
    using FrameBufferImageSource = std::variant<typed::ResourceId, ImageAccessor>;
    struct FrameBufferInit {
        PipelinePassSource pass;
        std::uint32_t width = 0, height = 0, layers = 1;
        // 全sourceを永続依存とする。行=native framebuffer、列=AttachmentId。
        std::vector<std::vector<FrameBufferImageSource>> attachments;
    };
    using CommandPoolSource = std::variant<typed::ResourceId, CommandPool>;
    struct CommandBufferInit {
        CommandPoolSource pool;
        std::uint32_t count = DEFAULT_BUFFER_SIZE;
    };
    using ResourceInit = std::variant<BufferCreateInfo, ImageCreateInfo, SamplerCreateInfo, BufferView, ImageAccessor, SamplerAccessor,
                                      DescriptorLayoutCreateInfo, DescriptorPoolCreateInfo, DescriptorSetInit,
                                      graphic_resource::BindingLayout, graphic_resource::Pool, ShaderCreateInfo, ShaderAccessor, PipelineLayoutInit, PipelineLayoutAccessor, RenderPassDescription, RenderPassAccessor,
                                      GraphicsPipelineInit, ComputePipelineInit, PipelineAccessor, FrameBufferInit, FrameBufferAccessor,
                                      CommandPoolCreateInfo, CommandBufferInit, CommandPool, CommandBuffer>;
    class ResourceArena {
        std::shared_ptr<detail::ResourceArenaOwner> _owner;
        ResourceArena(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                      std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                      std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment,
                      std::span<const LogicalUseState> dependencies, DependencyPolicy policy);

    public:
        template<size_t capacity>
        ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers);
        template<size_t capacity>
        ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, DependencyPolicy policy);
        template<size_t capacity>
        ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, storage::ArenaMemory memory,
                      std::span<const LogicalUseState> dependencies = {}, DependencyPolicy policy = DependencyPolicy::FIXED);
        std::shared_ptr<void> owner() const;
        OwnerRef owner_ref() const;
        size_t size() const;
        size_t slots() const;
        typed::LifetimeGroup lifetime() const;
        BufferView buffer(typed::ResourceId id) const;
        ImageAccessor image(typed::ResourceId id) const;
        SamplerAccessor sampler(typed::ResourceId id) const;
        graphic_resource::BindingLayout descriptor_layout(typed::ResourceId id) const;
        graphic_resource::Pool descriptor_pool(typed::ResourceId id) const;
        GraphicResource descriptor_set(typed::ResourceId id) const;
        ShaderAccessor shader(typed::ResourceId id) const;
        PipelineLayoutAccessor pipeline_layout(typed::ResourceId id) const;
        RenderPassAccessor render_pass(typed::ResourceId id) const;
        PipelineAccessor pipeline(typed::ResourceId id) const;
        FrameBufferAccessor frame_buffer(typed::ResourceId id) const;
        CommandPool command_pool(typed::ResourceId id) const;
        CommandBuffer command_buffer(typed::ResourceId id) const;
        // 非所有の物理Buffer全体。外部の部分viewに対してはbuffer()を使用する。
        BufferRef borrow_buffer(typed::ResourceId id) const&;
        BufferRef borrow_buffer(typed::ResourceId id) const&& = delete;
    };

    template<size_t capacity>
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers)
        : ResourceArena(device, plan, initializers, storage::ArenaMemory(plan.bytes(), plan.alignment())) {}
    template<size_t capacity>
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, DependencyPolicy policy)
        : ResourceArena(device, plan, initializers, storage::ArenaMemory(plan.bytes(), plan.alignment()), {}, policy) {}
    template<size_t capacity>
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, const typed::ResourcePlan<capacity>& plan, std::span<const ResourceInit> initializers, storage::ArenaMemory memory,
                                 std::span<const LogicalUseState> dependencies, DependencyPolicy policy)
        : ResourceArena((plan.require_region(memory.size(), memory.alignment()), device), plan.lifetime(), plan.placements(), initializers, memory._region, plan.bytes(), plan.alignment(), dependencies, policy) {}
} // namespace VSL_NAMESPACE::lifetime
