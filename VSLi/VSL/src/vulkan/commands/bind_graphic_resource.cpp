//
// Created by morimoto_hibiki on 2025/07/30.
//

#include <VSL/lifetime/descriptor_access.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_graphic_resource.hpp>
#include <VSL/lifetime/submission_retention.hpp>

#include <ranges>
#include <utility>

namespace {
    using VSL_NAMESPACE::lifetime::detail::DescriptorAccess;
}

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
    if (resources.empty() || !pipeline)
        return;
    const auto resources = this->resources;
    const auto pipeline = *this->pipeline;
    const auto destination = this->destination;
    const auto first_binding = this->first_binding;
    const auto use = _use;
    Validate(resources);
    if (!pipeline.data_ || !pipeline.data_->pipeline || !pipeline.data_->layout)
        throw std::logic_error("Descriptor bind requires a pipeline");
    const auto native_sets = resources | std::views::transform([](const auto& resource) { return DescriptorAccess::Owned(resource)->descriptor_set; }) | std::ranges::to<std::vector<vk::DescriptorSet>>();
    const auto layout = pipeline.data_->layout->pipeline_layout;
    const NativeRecordingContext recording(buffer);
    Track(resources, use, buffer, recording);
    recording.retain(pipeline.owner_ref(), ResourceRetention::SUBMISSION);
    vk::CommandBuffer(recording.native_command_buffer()).bindDescriptorSets(static_cast<vk::PipelineBindPoint>(destination), layout, first_binding, native_sets, {});
}

void VSL_NAMESPACE::command::BindGraphicResource::set_pipeline(std::optional<PipelineAccessor> pipeline) {
    this->pipeline = pipeline ? pipeline : _fallback_pipeline;
}

void VSL_NAMESPACE::command::BindGraphicResource::track(const CommandBuffer& buffer) const {
    const auto resources = this->resources;
    const auto use = _use;
    if (resources.empty() && !use)
        return;
    Validate(resources);
    const NativeRecordingContext recording(buffer);
    Track(resources, use, buffer, recording);
}

void VSL_NAMESPACE::command::BindGraphicResource::Track(const std::vector<GraphicResource>& resources,
                                                        const std::shared_ptr<ResourceUseGroup>& use, const CommandBuffer& buffer,
                                                        const NativeRecordingContext& recording) {
    if (use) {
        use->record(buffer);
        recording.require_current();
    }
    for (const auto& resource : resources) {
        if (use && use->tracks_descriptor(resource)) {
            use->retain_descriptor_owner(resource, buffer);
            recording.require_current();
            continue;
        }
        recording.retain(resource.owner_ref(), ResourceRetention::SUBMISSION);
        lifetime::record_descriptor_resources(*DescriptorAccess::Owned(resource), buffer);
        recording.require_current();
    }
}

void VSL_NAMESPACE::command::BindGraphicResource::Validate(const std::vector<GraphicResource>& resources) {
    for (const auto& resource : resources) {
        const auto& owner = DescriptorAccess::Owned(resource);
        if (!owner || !owner.use_count())
            throw std::logic_error("Descriptor bind requires an owning resource");
        lifetime::require_descriptor_valid(*owner);
    }
}
