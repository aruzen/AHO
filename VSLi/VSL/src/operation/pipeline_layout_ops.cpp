#include <VSL/operation/pipeline_layout_ops.hpp>
#include <algorithm>
#include <bit>
#include <map>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    namespace {
        std::uint32_t count(size_t size);
        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> signatures(
            const impl::LogicalDeviceImpl& device, std::span<impl::ResourceBindingLayoutImpl* const> layouts);
        std::vector<vk::PushConstantRange> native_ranges(const impl::LogicalDeviceImpl& device, std::span<const PushConstantRange> ranges);
        std::uint32_t count(size_t size) {
            if (size > UINT32_MAX)
                throw std::length_error("Pipeline layout count exceeds uint32_t");
            return static_cast<std::uint32_t>(size);
        }
        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> signatures(
            const impl::LogicalDeviceImpl& device, std::span<impl::ResourceBindingLayoutImpl* const> layouts) {
            if (layouts.size() > device.parent_device->device.getProperties().limits.maxBoundDescriptorSets)
                throw std::out_of_range("Pipeline descriptor set count exceeds the device limit");
            std::vector<std::vector<vk::DescriptorSetLayoutBinding>> result;
            result.reserve(layouts.size());
            for (const auto* layout : layouts) {
                if (!layout || !layout->layout || layout->device != &device)
                    throw std::invalid_argument("Pipeline descriptor layouts require the same live device");
                auto& bindings = result.emplace_back(layout->layout_bindings);
                std::ranges::sort(bindings, {}, &vk::DescriptorSetLayoutBinding::binding);
            }
            return result;
        }
        std::vector<vk::PushConstantRange> native_ranges(const impl::LogicalDeviceImpl& device, std::span<const PushConstantRange> ranges) {
            const auto normalized = normalize_push_ranges(device, ranges);
            std::vector<vk::PushConstantRange> result;
            result.reserve(normalized.size());
            for (const auto& range : normalized)
                result.push_back({.stageFlags = vk::ShaderStageFlags(static_cast<std::uint32_t>(range.target_shader)),
                                  .offset = range.offset,
                                  .size = range.size});
            return result;
        }
    } // namespace
    void validate_pipeline_device(const impl::LogicalDeviceImpl& device) {
        if (!device.device || !device.parent_device || !device.parent_device->device)
            throw std::invalid_argument("Pipeline layout requires a live logical and physical device");
    }
    std::vector<PushConstantRange> normalize_push_ranges(const impl::LogicalDeviceImpl& device, std::span<const PushConstantRange> definitions) {
        validate_pipeline_device(device);
        const auto limit = device.parent_device->device.getProperties().limits.maxPushConstantsSize;
        std::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> per_stage;
        for (const auto& definition : definitions) {
            auto stages = static_cast<std::uint32_t>(definition.target_shader);
            if (!stages || (stages & ~0x3fffu) || !definition.size || definition.offset >= limit || definition.size > limit - definition.offset)
                throw std::invalid_argument("Invalid push constant range");
            const auto end = definition.offset + definition.size;
            for (; stages; stages &= stages - 1) {
                const auto stage = std::uint32_t{1} << std::countr_zero(stages);
                auto& bounds = per_stage.try_emplace(stage, definition.offset, end).first->second;
                bounds.first = std::min(bounds.first, definition.offset);
                bounds.second = std::max(bounds.second, end);
            }
        }
        std::vector<PushConstantRange> result;
        result.reserve(per_stage.size());
        for (const auto& [stage, bounds] : per_stage) {
            if (bounds.first % 4 || (bounds.second - bounds.first) % 4)
                throw std::invalid_argument("Push constant range must be aligned to four bytes");
            result.push_back({static_cast<ShaderFlag>(stage), bounds.first, bounds.second - bounds.first});
        }
        return result;
    }
    void initialize_pipeline_layout(impl::PipelineLayoutImpl& target, impl::LogicalDeviceImpl& device,
                                    std::span<impl::ResourceBindingLayoutImpl* const> layouts, std::span<const PushConstantRange> ranges) {
        target.device = &device;
        validate_pipeline_device(device);
        target.native_device = device.device;
        target.binding_signatures = signatures(device, layouts);
        target.push_ranges = native_ranges(device, ranges);
        std::vector<vk::DescriptorSetLayout> handles;
        handles.reserve(layouts.size());
        for (const auto* layout : layouts)
            handles.push_back(layout->layout);
        const vk::PipelineLayoutCreateInfo info{.setLayoutCount = count(handles.size()), .pSetLayouts = handles.data(), .pushConstantRangeCount = count(target.push_ranges.size()), .pPushConstantRanges = target.push_ranges.data()};
        target.pipeline_layout = device.device.createPipelineLayout(info);
    }
    bool pipeline_layout_matches(const impl::PipelineLayoutImpl& target, std::span<impl::ResourceBindingLayoutImpl* const> layouts,
                                 std::span<const PushConstantRange> ranges) {
        return target.binding_signatures == signatures(*target.device, layouts) && target.push_ranges == native_ranges(*target.device, ranges);
    }
    void destroy_pipeline_layout(impl::PipelineLayoutImpl& target) noexcept {
        if (target.pipeline_layout)
            target.native_device.destroyPipelineLayout(target.pipeline_layout);
        target.pipeline_layout = nullptr;
    }
    PipelineScratch::PipelineScratch(const impl::PipelineRecipe& recipe) : color_blend(recipe.color_blend) {
        dynamic_state.dynamicStateCount = count(recipe.enabled_dynamic_states.size());
        dynamic_state.pDynamicStates = recipe.enabled_dynamic_states.data();
        vertex_input.vertexBindingDescriptionCount = count(recipe.vertex_bindings.size());
        vertex_input.pVertexBindingDescriptions = recipe.vertex_bindings.data();
        vertex_input.vertexAttributeDescriptionCount = count(recipe.vertex_attributes.size());
        vertex_input.pVertexAttributeDescriptions = recipe.vertex_attributes.data();
        viewport.viewportCount = count(recipe.viewports.size());
        viewport.pViewports = recipe.viewports.data();
        viewport.scissorCount = count(recipe.scissors.size());
        viewport.pScissors = recipe.scissors.data();
        color_blend.attachmentCount = count(recipe.blend_attachments.size());
        color_blend.pAttachments = recipe.blend_attachments.data();
    }
} // namespace VSL_NAMESPACE::operation
