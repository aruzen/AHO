//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/vulkan/stages/push_contents.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <algorithm>
#include <bit>
#include <map>
#include <stdexcept>

void VSL_NAMESPACE::pipeline_layout::PushConstants::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = *pl.data_->info;
    const auto limit = pl.data_->device->parent_device->device.getProperties().limits.maxPushConstantsSize;
    std::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> per_stage;
    for (const auto& definition : definitions) {
        auto stages = static_cast<std::uint32_t>(definition.target_shader);
        if (!stages || !definition.size || definition.offset >= limit || definition.size > limit - definition.offset)
            throw std::invalid_argument("Invalid push constant range");
        const auto end = definition.offset + definition.size;
        for (; stages; stages &= stages - 1) {
            const auto stage = std::uint32_t{1} << std::countr_zero(stages);
            auto& bounds = per_stage.try_emplace(stage, definition.offset, end).first->second;
            bounds.first = std::min(bounds.first, definition.offset);
            bounds.second = std::max(bounds.second, end);
        }
    }

    std::vector<vk::PushConstantRange> ranges;
    ranges.reserve(per_stage.size());
    for (const auto& [stage, bounds] : per_stage) {
        if (bounds.first % 4 || (bounds.second - bounds.first) % 4)
            throw std::invalid_argument("Push constant range must be aligned to four bytes");
        ranges.push_back({.stageFlags = vk::ShaderStageFlags(stage), .offset = bounds.first, .size = bounds.second - bounds.first});
    }

    std::any next = std::move(ranges);
    auto& storage = info.pool["colorBlendAttachment"];
    storage.swap(next);
    const auto& values = std::any_cast<const std::vector<vk::PushConstantRange>&>(storage);
    info.pipeline_layout.pPushConstantRanges = values.empty() ? nullptr : values.data();
    info.pipeline_layout.pushConstantRangeCount = static_cast<std::uint32_t>(values.size());
}
