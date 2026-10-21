#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../pipeline.hpp"

namespace VSL_NAMESPACE::command {
    struct BindPipeline : public __Command, public __PipelineHolder {
        BindPipeline(PipelineAccessor pipeline);

        PipelineAccessor pipeline;

        PipelineAccessor getPipeline() override;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
} // namespace VSL_NAMESPACE::command
