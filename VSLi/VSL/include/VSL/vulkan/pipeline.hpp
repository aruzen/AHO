#pragma once

#include "../define.hpp"
#include "pv.hpp"

#include "pipeline_layout.hpp"
#include "render_pass.hpp"

namespace VSL_NAMESPACE {
    struct PipelineAccessor {
        std::shared_ptr<_impl::Pipeline_impl> _data;
    };

    struct GraphicsPipeline : public PipelineAccessor {
        GraphicsPipeline(PipelineLayoutAccessor layout, RenderPass pass);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    struct ComputePipeline : public PipelineAccessor {
        ComputePipeline(PipelineLayoutAccessor layout);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
}