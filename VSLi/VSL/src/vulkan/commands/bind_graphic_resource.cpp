//
// Created by morimoto_hibiki on 2025/07/30.
//

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_graphic_resource.hpp>

#include <ranges>
#include <utility>
#include <utility>

VSL_NAMESPACE::command::BindGraphicResource::BindGraphicResource(VSL_NAMESPACE::GraphicResource resource,
                                                                 VSL_NAMESPACE::graphic_resource::BindingDestination dst,
                                                                 std::optional<PipelineAccessor> pipeline,
                                                                 std::uint32_t first_binding)
    : resources{resource}, destination(dst), pipeline(std::move(pipeline)), first_binding(first_binding) {}

VSL_NAMESPACE::command::BindGraphicResource::BindGraphicResource(const std::vector<GraphicResource>& resources,
                                                                 VSL_NAMESPACE::graphic_resource::BindingDestination dst,
                                                                 std::optional<PipelineAccessor> pipeline,
                                                                 std::uint32_t first_binding)
    : resources(resources), destination(dst), pipeline(std::move(pipeline)), first_binding(first_binding) {}

void VSL_NAMESPACE::command::BindGraphicResource::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    if (pipeline) {
        for (const auto& resource : resources) {
            if (!resource._data || !resource._data->pool || !resource._data->pool->descriptorPool || !resource._data->descriptorSet)
                throw std::logic_error("Descriptor resource has been destroyed");
            buffer.retain(resource._data);
        }
        buffer.retain(pipeline->_data);
        buffer._data->commandBuffers[buffer.getCurrentBufferIdx()]
            .bindDescriptorSets((vk::PipelineBindPoint)destination,
                                pipeline.value()._data->layout->pipelineLayout,
                                first_binding,
                                resources | std::views::transform([](auto& r) { return r._data->descriptorSet; }) | std::ranges::to<std::vector<vk::DescriptorSet>>(),
                                {});
    }
}

void VSL_NAMESPACE::command::BindGraphicResource::setPipeline(std::optional<PipelineAccessor> pipeline) {
    this->pipeline = pipeline;
}
