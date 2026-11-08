#pragma once
#include "buffer_layout.hpp"
#include "../vulkan/pv.hpp"

namespace VSL_NAMESPACE::storage {
    template<typename Impl>
    struct ImplLayout;
    template<>
    struct ImplLayout<impl::VulkanImplAccessor> {
        static constexpr size_t SIZE = INSTANCE_IMPL_SIZE, ALIGNMENT = INSTANCE_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::PhysicalDeviceImpl> {
        static constexpr size_t SIZE = PHYSICAL_DEVICE_IMPL_SIZE, ALIGNMENT = PHYSICAL_DEVICE_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::BufferImpl> {
        static constexpr size_t SIZE = BUFFER_IMPL_SIZE, ALIGNMENT = BUFFER_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::ImageImpl> {
        static constexpr size_t SIZE = IMAGE_IMPL_SIZE, ALIGNMENT = IMAGE_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::SamplerImpl> {
        static constexpr size_t SIZE = SAMPLER_IMPL_SIZE, ALIGNMENT = SAMPLER_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::ResourceBindingLayoutImpl> {
        static constexpr size_t SIZE = DESCRIPTOR_LAYOUT_IMPL_SIZE, ALIGNMENT = DESCRIPTOR_LAYOUT_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::GraphicResourcePoolImpl> {
        static constexpr size_t SIZE = DESCRIPTOR_POOL_IMPL_SIZE, ALIGNMENT = DESCRIPTOR_POOL_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::GraphicResourceImpl> {
        static constexpr size_t SIZE = DESCRIPTOR_SET_IMPL_SIZE, ALIGNMENT = DESCRIPTOR_SET_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::ShaderImpl> {
        static constexpr size_t SIZE = SHADER_IMPL_SIZE, ALIGNMENT = SHADER_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::PipelineLayoutImpl> {
        static constexpr size_t SIZE = PIPELINE_LAYOUT_IMPL_SIZE, ALIGNMENT = PIPELINE_LAYOUT_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::RenderPassImpl> {
        static constexpr size_t SIZE = RENDER_PASS_IMPL_SIZE, ALIGNMENT = RENDER_PASS_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::PipelineImpl> {
        static constexpr size_t SIZE = PIPELINE_IMPL_SIZE, ALIGNMENT = PIPELINE_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::FrameBufferImpl> {
        static constexpr size_t SIZE = FRAME_BUFFER_IMPL_SIZE, ALIGNMENT = FRAME_BUFFER_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::CommandPoolImpl> {
        static constexpr size_t SIZE = COMMAND_POOL_IMPL_SIZE, ALIGNMENT = COMMAND_POOL_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::CommandBufferImpl> {
        static constexpr size_t SIZE = COMMAND_BUFFER_IMPL_SIZE, ALIGNMENT = COMMAND_BUFFER_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::SemaphoreImpl> {
        static constexpr size_t SIZE = SEMAPHORE_IMPL_SIZE, ALIGNMENT = SEMAPHORE_IMPL_ALIGNMENT;
    };
    template<>
    struct ImplLayout<impl::FenceImpl> {
        static constexpr size_t SIZE = FENCE_IMPL_SIZE, ALIGNMENT = FENCE_IMPL_ALIGNMENT;
    };
    inline constexpr size_t NO_SEPARATE_USE = std::numeric_limits<size_t>::max();
    template<typename Impl>
    struct ResourceSlotLayout {
        static constexpr size_t USE_OFFSET = checked_align(ImplLayout<Impl>::SIZE, BUFFER_USE_ALIGNMENT);
        static constexpr size_t ALIGNMENT = ImplLayout<Impl>::ALIGNMENT > BUFFER_USE_ALIGNMENT ? ImplLayout<Impl>::ALIGNMENT : BUFFER_USE_ALIGNMENT;
        static constexpr size_t SIZE = checked_align(USE_OFFSET + BUFFER_USE_SIZE, ALIGNMENT);
    };
    template<>
    struct ResourceSlotLayout<impl::ResourceBindingLayoutImpl> {
        static constexpr size_t SIZE = DESCRIPTOR_LAYOUT_IMPL_SIZE, ALIGNMENT = DESCRIPTOR_LAYOUT_IMPL_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::GraphicResourcePoolImpl> {
        static constexpr size_t SIZE = DESCRIPTOR_POOL_IMPL_SIZE, ALIGNMENT = DESCRIPTOR_POOL_IMPL_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::GraphicResourceImpl> {
        static constexpr size_t SIZE = DESCRIPTOR_SET_IMPL_SIZE, ALIGNMENT = DESCRIPTOR_SET_IMPL_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::ShaderImpl> {
        static constexpr size_t SIZE = SHADER_IMPL_SIZE, ALIGNMENT = SHADER_IMPL_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::PipelineLayoutImpl> {
        static constexpr size_t SIZE = PIPELINE_LAYOUT_IMPL_SIZE, ALIGNMENT = PIPELINE_LAYOUT_IMPL_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::RenderPassImpl> {
        static constexpr size_t SIZE = RENDER_PASS_IMPL_SIZE, ALIGNMENT = RENDER_PASS_IMPL_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::PipelineImpl> {
        static constexpr size_t SIZE = PIPELINE_IMPL_SIZE, ALIGNMENT = PIPELINE_IMPL_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::FrameBufferImpl> {
        // Implに隣接するlifetime contextを含む。GPU UseStateではない。
        static constexpr size_t SIZE = FRAME_BUFFER_SLOT_SIZE, ALIGNMENT = FRAME_BUFFER_SLOT_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::CommandPoolImpl> {
        static constexpr size_t SIZE = COMMAND_POOL_SLOT_SIZE, ALIGNMENT = COMMAND_POOL_SLOT_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::CommandBufferImpl> {
        static constexpr size_t SIZE = COMMAND_BUFFER_SLOT_SIZE, ALIGNMENT = COMMAND_BUFFER_SLOT_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::SemaphoreImpl> {
        static constexpr size_t SIZE = SEMAPHORE_SLOT_SIZE, ALIGNMENT = SEMAPHORE_SLOT_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::FenceImpl> {
        static constexpr size_t SIZE = FENCE_SLOT_SIZE, ALIGNMENT = FENCE_SLOT_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::VulkanImplAccessor> {
        static constexpr size_t SIZE = INSTANCE_SLOT_SIZE, ALIGNMENT = INSTANCE_SLOT_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    template<>
    struct ResourceSlotLayout<impl::PhysicalDeviceImpl> {
        static constexpr size_t SIZE = PHYSICAL_DEVICE_SLOT_SIZE, ALIGNMENT = PHYSICAL_DEVICE_SLOT_ALIGNMENT, USE_OFFSET = NO_SEPARATE_USE;
    };
    enum class ResourceKind : std::uint32_t {
        BUFFER,
        IMAGE,
        SAMPLER,
        DESCRIPTOR_LAYOUT,
        DESCRIPTOR_POOL,
        DESCRIPTOR_SET,
        SHADER,
        PIPELINE_LAYOUT,
        RENDER_PASS,
        PIPELINE,
        FRAME_BUFFER,
        COMMAND_POOL,
        COMMAND_BUFFER,
        SEMAPHORE,
        FENCE
    };
    struct SlotLayout {
        size_t size, alignment, use_offset;
    };
    constexpr SlotLayout resource_slot_layout(ResourceKind kind);
    constexpr size_t checked_add(size_t first, size_t second);
    namespace detail {
        void require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>);
    } // namespace detail

    constexpr SlotLayout resource_slot_layout(ResourceKind kind) {
        using Buffer = ResourceSlotLayout<impl::BufferImpl>;
        using Image = ResourceSlotLayout<impl::ImageImpl>;
        using Sampler = ResourceSlotLayout<impl::SamplerImpl>;
        using Layout = ResourceSlotLayout<impl::ResourceBindingLayoutImpl>;
        using Pool = ResourceSlotLayout<impl::GraphicResourcePoolImpl>;
        using Set = ResourceSlotLayout<impl::GraphicResourceImpl>;
        using Shader = ResourceSlotLayout<impl::ShaderImpl>;
        using PipelineLayout = ResourceSlotLayout<impl::PipelineLayoutImpl>;
        using RenderPass = ResourceSlotLayout<impl::RenderPassImpl>;
        using Pipeline = ResourceSlotLayout<impl::PipelineImpl>;
        using FrameBuffer = ResourceSlotLayout<impl::FrameBufferImpl>;
        using CommandPool = ResourceSlotLayout<impl::CommandPoolImpl>;
        using CommandBuffer = ResourceSlotLayout<impl::CommandBufferImpl>;
        switch (kind) {
            case ResourceKind::BUFFER:
                return {Buffer::SIZE, Buffer::ALIGNMENT, Buffer::USE_OFFSET};
            case ResourceKind::IMAGE:
                return {Image::SIZE, Image::ALIGNMENT, Image::USE_OFFSET};
            case ResourceKind::SAMPLER:
                return {Sampler::SIZE, Sampler::ALIGNMENT, Sampler::USE_OFFSET};
            case ResourceKind::DESCRIPTOR_LAYOUT:
                return {Layout::SIZE, Layout::ALIGNMENT, Layout::USE_OFFSET};
            case ResourceKind::DESCRIPTOR_POOL:
                return {Pool::SIZE, Pool::ALIGNMENT, Pool::USE_OFFSET};
            case ResourceKind::DESCRIPTOR_SET:
                return {Set::SIZE, Set::ALIGNMENT, Set::USE_OFFSET};
            case ResourceKind::SHADER:
                return {Shader::SIZE, Shader::ALIGNMENT, Shader::USE_OFFSET};
            case ResourceKind::PIPELINE_LAYOUT:
                return {PipelineLayout::SIZE, PipelineLayout::ALIGNMENT, PipelineLayout::USE_OFFSET};
            case ResourceKind::RENDER_PASS:
                return {RenderPass::SIZE, RenderPass::ALIGNMENT, RenderPass::USE_OFFSET};
            case ResourceKind::PIPELINE:
                return {Pipeline::SIZE, Pipeline::ALIGNMENT, Pipeline::USE_OFFSET};
            case ResourceKind::FRAME_BUFFER:
                return {FrameBuffer::SIZE, FrameBuffer::ALIGNMENT, FrameBuffer::USE_OFFSET};
            case ResourceKind::COMMAND_POOL:
                return {CommandPool::SIZE, CommandPool::ALIGNMENT, CommandPool::USE_OFFSET};
            case ResourceKind::COMMAND_BUFFER:
                return {CommandBuffer::SIZE, CommandBuffer::ALIGNMENT, CommandBuffer::USE_OFFSET};
            case ResourceKind::SEMAPHORE:
                return {ResourceSlotLayout<impl::SemaphoreImpl>::SIZE, ResourceSlotLayout<impl::SemaphoreImpl>::ALIGNMENT, NO_SEPARATE_USE};
            case ResourceKind::FENCE:
                return {ResourceSlotLayout<impl::FenceImpl>::SIZE, ResourceSlotLayout<impl::FenceImpl>::ALIGNMENT, NO_SEPARATE_USE};
        }
        throw std::invalid_argument("Unknown resource implementation kind");
    }
    constexpr size_t checked_add(size_t first, size_t second) {
        if (second > std::numeric_limits<size_t>::max() - first)
            throw std::overflow_error("Resource placement size overflow");
        return first + second;
    }
} // namespace VSL_NAMESPACE::storage
