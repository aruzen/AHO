#pragma once

#include "../define.hpp"
#include "pv.hpp"

#include "pipeline_layout.hpp"
#include "render_pass.hpp"
#include "command.hpp"

namespace VSL_NAMESPACE {
    struct PipelineAccessor {
        std::shared_ptr<impl::PipelineImpl> data_;
    };

    struct GraphicsPipeline : public PipelineAccessor {
        GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, std::uint32_t subpass = 0);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    struct ComputePipeline : public PipelineAccessor {
        ComputePipeline(PipelineLayoutAccessor layout);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
} // namespace VSL_NAMESPACE
