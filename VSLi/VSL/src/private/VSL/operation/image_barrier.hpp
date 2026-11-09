#pragma once
#include <VSL/operation/image_create_info.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::operation {
    struct ImageBarrier {
        vk::ImageMemoryBarrier barrier;
        vk::PipelineStageFlags source, destination;
    };
    ImageBarrier prepare_image_barrier(const impl::ImageImpl& image, ImageLayout old_layout, ImageLayout new_layout,
                                       std::uint32_t base_layer = 0, std::uint32_t layer_count = 0,
                                       std::uint32_t shader_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    void record_image_barrier(vk::CommandBuffer commands, const ImageBarrier& barrier);
} // namespace VSL_NAMESPACE::operation
