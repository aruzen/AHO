#pragma once

#include "../define.h"
#include "pv.h"

#include "pipeline_layout.h"
#include "render_pass.h"

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