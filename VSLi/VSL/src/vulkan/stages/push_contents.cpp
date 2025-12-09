//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/vulkan/stages/push_contents.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/_pimpls.hpp>

void vsl::pipeline_layout::PushConstants::injection(vsl::PipelineLayoutAccessor pl) {
    auto &info = *pl._data->info;

    auto &push_contents = std::any_cast<std::vector<vk::PushConstantRange> &>
            (info.pool["colorBlendAttachment"] = std::vector<vk::PushConstantRange>());

    std::map<vsl::ShaderFlag, std::uint32_t> per_type;
    for (auto &def: definitions) {
        if (not per_type.contains(def.targetShader))
            per_type[def.targetShader] = 0;
        per_type[def.targetShader] = (per_type[def.targetShader] < def.offset
                                      ? def.offset
                                      : per_type[def.targetShader]) + def.size;
    }

    std::uint32_t offset = 0;
    for (const auto &[type, size]: per_type) {
        push_contents.emplace_back(vk::PushConstantRange{
                .stageFlags = (vk::ShaderStageFlagBits) type,
                .offset = offset,
                .size = size
        });
        offset += size;
    }

    info.pipelineLayout.pPushConstantRanges = push_contents.data();
    info.pipelineLayout.pushConstantRangeCount = push_contents.size();
}
