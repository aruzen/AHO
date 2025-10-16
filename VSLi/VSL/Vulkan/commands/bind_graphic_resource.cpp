//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../_pimpls.h"

#include "bind_graphic_resource.hpp"


vsl::command::BindGraphicResource::BindGraphicResource(vsl::GraphicResource resource, vsl::graphic_resource::BindingDestination dst,
                                                       std::optional<PipelineAccessor> pipeline)
                                                       : resource(resource), destination(dst), pipeline(pipeline) {}

void VSL_NAMESPACE::command::BindGraphicResource::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)
{
    if (pipeline)
        buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
            .bindDescriptorSets((vk::PipelineBindPoint)destination,
                                pipeline.value()._data->layout->pipelineLayout,
                                0,
                                { resource._data->descriptorSet},
                                { });
}

void vsl::command::BindGraphicResource::setPipeline(std::optional<PipelineAccessor> pipeline) {
    this->pipeline = pipeline;
}