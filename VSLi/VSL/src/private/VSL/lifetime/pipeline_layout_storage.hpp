#pragma once
#include <VSL/operation/pipeline_layout_ops.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct PipelineLayoutSlotStorage {
        alignas(impl::PipelineLayoutImpl) std::byte value[sizeof(impl::PipelineLayoutImpl)];
    };
    size_t retain_recipe_owner(std::vector<OwnerRef>& owners, OwnerRef owner);
    // CPU領域・Deviceの所有は呼出側。DescriptorLayoutは構築呼出し中のみ必要。
    void construct_pipeline_layout(void* storage, LogicalDeviceAccessor device,
                                   std::span<impl::ResourceBindingLayoutImpl* const> layouts, std::span<const PushConstantRange> ranges);
    void destroy_pipeline_layout(void* storage) noexcept;
    impl::PipelineLayoutImpl& pipeline_layout_at(void* storage);
    std::shared_ptr<impl::PipelineLayoutImpl> make_heap_pipeline_layout(LogicalDeviceAccessor device,
                                                                        std::span<impl::ResourceBindingLayoutImpl* const> layouts, std::span<const PushConstantRange> ranges);
} // namespace VSL_NAMESPACE::lifetime
