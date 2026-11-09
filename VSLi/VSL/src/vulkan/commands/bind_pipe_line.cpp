#include <VSL/lifetime/command_access.hpp>
#include <VSL/vulkan/commands/bind_pipe_line.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
VSL_NAMESPACE::command::BindPipeline::BindPipeline(PipelineAccessor pipeline) : pipeline(pipeline) {}

VSL_NAMESPACE::PipelineAccessor VSL_NAMESPACE::command::BindPipeline::get_pipeline() {
    return pipeline;
}

void VSL_NAMESPACE::command::BindPipeline::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.retain(pipeline.data_);
    CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].bindPipeline(pipeline.data_->bind_point, pipeline.data_->pipeline);
}
