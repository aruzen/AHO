#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../pipeline.hpp"

namespace VSL_NAMESPACE::command {
    struct BindPipeline : public Command, public PipelineHolder {
        BindPipeline(PipelineAccessor pipeline);

        PipelineAccessor pipeline;

        PipelineAccessor get_pipeline() override;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
} // namespace VSL_NAMESPACE::command
