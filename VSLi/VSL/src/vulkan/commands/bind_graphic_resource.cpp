//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_graphic_resource.hpp>

#include <ranges>


vsl::command::BindGraphicResource::BindGraphicResource(vsl::GraphicResource resource, vsl::graphic_resource::BindingDestination dst,
                                                       std::optional<PipelineAccessor> pipeline)
        : resources{resource}, destination(dst), pipeline(pipeline) {}

vsl::command::BindGraphicResource::BindGraphicResource(const std::vector<GraphicResource> &resources,
                                                       vsl::graphic_resource::BindingDestination dst,
                                                       std::optional<PipelineAccessor> pipeline)
        : resources(resources), destination(dst), pipeline(pipeline) {}
void VSL_NAMESPACE::command::BindGraphicResource::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager)

{
    if (pipeline)
        buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
            .bindDescriptorSets((vk::PipelineBindPoint)destination,
                                pipeline.value()._data->layout->pipelineLayout,
                                0,
                                resources
                                | std::views::transform([](auto &r) { return r._data->descriptorSet; })
                                | std::ranges::to<std::vector<vk::DescriptorSet>>(),
                                { });
}

void vsl::command::BindGraphicResource::setPipeline(std::optional<PipelineAccessor> pipeline) {
    this->pipeline = pipeline;
}