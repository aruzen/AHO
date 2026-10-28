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
                                                                 std::uint32_t first_binding, std::shared_ptr<ResourceUseGroup> use)
    : resources{resource}, destination(dst), pipeline(std::move(pipeline)), first_binding(first_binding), _use(std::move(use)) {}

VSL_NAMESPACE::command::BindGraphicResource::BindGraphicResource(const std::vector<GraphicResource>& resources,
                                                                 VSL_NAMESPACE::graphic_resource::BindingDestination dst,
                                                                 std::optional<PipelineAccessor> pipeline,
                                                                 std::uint32_t first_binding, std::shared_ptr<ResourceUseGroup> use)
    : resources(resources), destination(dst), pipeline(std::move(pipeline)), first_binding(first_binding), _use(std::move(use)) {}

void VSL_NAMESPACE::command::BindGraphicResource::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    if (resources.empty())
        return;
    if (pipeline) {
        for (const auto& resource : resources) {
            if (!resource.data_ || !resource.data_->pool || !resource.data_->pool->descriptor_pool || !resource.data_->descriptor_set)
                throw std::logic_error("Descriptor resource has been destroyed");
            buffer.retain(resource.data_);
        }
        track(buffer);
        buffer.retain(pipeline->data_);
        buffer.data_->command_buffers[buffer.get_current_buffer_idx()]
            .bindDescriptorSets((vk::PipelineBindPoint)destination,
                                pipeline.value().data_->layout->pipeline_layout,
                                first_binding,
                                resources | std::views::transform([](auto& r) { return r.data_->descriptor_set; }) | std::ranges::to<std::vector<vk::DescriptorSet>>(),
                                {});
    }
}

void VSL_NAMESPACE::command::BindGraphicResource::set_pipeline(std::optional<PipelineAccessor> pipeline) {
    this->pipeline = pipeline ? pipeline : _fallback_pipeline;
}

void VSL_NAMESPACE::command::BindGraphicResource::track(const CommandBuffer& buffer) const {
    if (_use)
        _use->record(buffer);
}
