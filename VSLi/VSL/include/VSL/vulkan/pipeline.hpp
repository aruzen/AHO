#pragma once

#include "../define.hpp"
#include "pv.hpp"

#include "pipeline_layout.hpp"
#include "render_pass.hpp"
#include "command.hpp"
#include <span>

namespace VSL_NAMESPACE {
    struct PipelineAccessor {
        std::shared_ptr<impl::PipelineImpl> data_;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
    };

    struct GraphicsPipeline : public PipelineAccessor {
        GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, std::uint32_t subpass = 0);
        GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, const GraphicsPipelineState& state,
                         std::span<const ShaderAccessor> shaders, std::uint32_t subpass = 0);
        explicit GraphicsPipeline(PipelineAccessor source);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    struct ComputePipeline : public PipelineAccessor {
        ComputePipeline(PipelineLayoutAccessor layout);
        ComputePipeline(PipelineLayoutAccessor layout, ShaderAccessor shader);
        explicit ComputePipeline(PipelineAccessor source);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
} // namespace VSL_NAMESPACE
