#pragma once
#include <VSL/operation/pipeline_layout_ops.hpp>
#include <VSL/operation/shader_ops.hpp>

namespace VSL_NAMESPACE::operation {
    // 生成呼出し中だけの置換境界。完成した実体には保存しない。
    struct GraphicsPipelineNativeFactory {
        void* context = nullptr;
        vk::Result (*create)(void*, vk::Device, vk::PipelineCache, const vk::GraphicsPipelineCreateInfo&, vk::Pipeline*) = nullptr;
    };
    class ScopedGraphicsPipelineNativeFactory {
        GraphicsPipelineNativeFactory _factory;
        const GraphicsPipelineNativeFactory* _previous;

    public:
        explicit ScopedGraphicsPipelineNativeFactory(GraphicsPipelineNativeFactory factory) noexcept;
        ScopedGraphicsPipelineNativeFactory(const ScopedGraphicsPipelineNativeFactory&) = delete;
        ScopedGraphicsPipelineNativeFactory& operator=(const ScopedGraphicsPipelineNativeFactory&) = delete;
        ~ScopedGraphicsPipelineNativeFactory();
    };
    // 全参照は解決済みで、生成呼出し中だけ借用する。
    struct GraphicsPipelineInput {
        impl::PipelineLayoutImpl* layout = nullptr;
        impl::RenderPassImpl* pass = nullptr;
        const impl::GraphicsPipelineStateData* state = nullptr;
        std::span<impl::ShaderImpl* const> shaders;
        std::uint32_t subpass = 0;
    };
    struct ComputePipelineInput {
        impl::PipelineLayoutImpl* layout = nullptr;
        impl::ShaderImpl* shader = nullptr;
    };
    void initialize_pipeline(impl::PipelineImpl& target, impl::LogicalDeviceImpl& device, const GraphicsPipelineInput& input);
    void initialize_pipeline(impl::PipelineImpl& target, impl::LogicalDeviceImpl& device, const ComputePipelineInput& input);
    void destroy_pipeline(impl::PipelineImpl& target) noexcept;
} // namespace VSL_NAMESPACE::operation
