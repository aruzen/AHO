#pragma once
#include <VSL/operation/pipeline_ops.hpp>
#include <VSL/vulkan/device.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct PipelineSlotStorage {
        alignas(impl::PipelineImpl) std::byte value[sizeof(impl::PipelineImpl)];
    };
    // Layoutは呼出側rootが永続保持し、生成入力のState/Shader/Passは呼出中だけ借用する。
    void construct_pipeline(void* storage, LogicalDeviceAccessor device, const operation::GraphicsPipelineInput& input);
    void construct_pipeline(void* storage, impl::LogicalDeviceImpl& device, const operation::GraphicsPipelineInput& input);
    void construct_pipeline(void* storage, LogicalDeviceAccessor device, const operation::ComputePipelineInput& input);
    void construct_pipeline(void* storage, impl::LogicalDeviceImpl& device, const operation::ComputePipelineInput& input);
    impl::PipelineImpl& pipeline_at(void* storage);
    void destroy_pipeline(void* storage) noexcept;
    std::shared_ptr<impl::PipelineImpl> make_heap_pipeline(LogicalDeviceAccessor device, OwnerRef layout, const operation::GraphicsPipelineInput& input);
    std::shared_ptr<impl::PipelineImpl> make_heap_pipeline(LogicalDeviceAccessor device, OwnerRef layout, const operation::ComputePipelineInput& input);
} // namespace VSL_NAMESPACE::lifetime
