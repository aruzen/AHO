#include <VSL/utils/typed_push_constants.hpp>
#include <VSL/utils/spir_reflector.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>
#include <algorithm>

namespace VSL_NAMESPACE::typed::detail {
    void inject_push_schema(PipelineLayoutEditAccessor layout, std::span<const PushRequirement> requirements) {
        pipeline_layout::PushConstants pushes;
        for (const auto& requirement : requirements)
            pushes.definitions.push_back(requirement.range);
        pushes.injection(layout);
    }
    void validate_push_schema(PipelineLayoutAccessor layout, std::span<const PushRequirement> requirements) {
        for (const auto& shader : layout.data_->info->shaders) {
            utils::SPIRVReflector reflection({}, std::span<const std::uint32_t>(shader->code), shader->name, ShaderType(shader->shader_stage_create_info.stage));
            if (!reflection.generated.push_constants)
                continue;
            for (const auto& member : reflection.generated.push_constants->definitions) {
                std::uint64_t cursor = member.offset;
                const auto end = cursor + member.size;
                while (cursor < end) {
                    auto next = cursor;
                    for (const auto& requirement : requirements) {
                        const auto& range = requirement.range;
                        if (contain(range.target_shader, member.target_shader) && range.offset <= cursor)
                            next = std::max(next, std::uint64_t(range.offset) + range.size);
                    }
                    if (next == cursor)
                        throw std::invalid_argument("Typed push shader mismatch: entry '" + shader->name + "', offset " + std::to_string(member.offset));
                    cursor = next;
                }
            }
        }
    }
    void validate_push_values(std::span<const PushRequirement> requirements, std::span<const std::byte> values) {
        for (const auto& requirement : requirements)
            if (requirement.data_offset > values.size() || requirement.range.size > values.size() - requirement.data_offset)
                throw std::out_of_range("Typed push storage does not cover its schema");
        for (size_t i = 0; i < requirements.size(); ++i) {
            const auto& a = requirements[i];
            for (size_t j = 0; j < i; ++j) {
                const auto& b = requirements[j];
                const auto begin = std::max(a.range.offset, b.range.offset);
                const auto end = std::min(std::uint64_t(a.range.offset) + a.range.size, std::uint64_t(b.range.offset) + b.range.size);
                if (begin < end && std::memcmp(values.data() + a.data_offset + begin - a.range.offset,
                                               values.data() + b.data_offset + begin - b.range.offset, end - begin) != 0)
                    throw std::invalid_argument("Overlapping typed push payload bytes must match across shader stages");
            }
        }
    }
    void invoke_push_values(PipelineAccessor pipeline, CommandBuffer buffer, std::span<const PushRequirement> requirements, std::span<const std::byte> values) {
        validate_push_values(requirements, values);
        buffer.retain(pipeline.data_);
        const auto& layout = pipeline.data_->layout;
        const auto& info = layout->info->pipeline_layout;
        const std::span native_ranges(info.pPushConstantRanges, info.pushConstantRangeCount);
        for (const auto& requirement : requirements) {
            const auto& range = requirement.range;
            auto cursor = range.offset;
            const auto end = cursor + range.size;
            while (cursor < end) {
                auto next = end;
                vk::ShaderStageFlags stages{};
                for (const auto& native : native_ranges) {
                    const auto native_end = native.offset + native.size;
                    if (native.offset <= cursor && cursor < native_end)
                        stages |= native.stageFlags;
                    if (cursor < native.offset)
                        next = std::min(next, native.offset);
                    if (cursor < native_end)
                        next = std::min(next, native_end);
                }
                if (!stages)
                    throw std::logic_error("Typed push schema is not covered by its native layout");
                buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pushConstants(layout->pipeline_layout,
                                                                                             stages, cursor, next - cursor, values.data() + requirement.data_offset + cursor - range.offset);
                cursor = next;
            }
        }
    }
} // namespace VSL_NAMESPACE::typed::detail
