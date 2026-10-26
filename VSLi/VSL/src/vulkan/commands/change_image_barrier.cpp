#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include "../image_format.hpp"
#include <array>
#include <utility>

namespace VSL_NAMESPACE::command {
    namespace {
        constexpr std::array<std::pair<ShaderFlag, vk::PipelineStageFlagBits>, 14> SHADER_READ_STAGES{{
            {ShaderFlag::VERTEX, vk::PipelineStageFlagBits::eVertexShader},
            {ShaderFlag::TESSELLATION_CONTROL, vk::PipelineStageFlagBits::eTessellationControlShader},
            {ShaderFlag::TESSELLATION_EVALUATION, vk::PipelineStageFlagBits::eTessellationEvaluationShader},
            {ShaderFlag::GEOMETRY, vk::PipelineStageFlagBits::eGeometryShader},
            {ShaderFlag::FRAGMENT, vk::PipelineStageFlagBits::eFragmentShader},
            {ShaderFlag::COMPUTE, vk::PipelineStageFlagBits::eComputeShader},
            {ShaderFlag::TASK, vk::PipelineStageFlagBits::eTaskShaderEXT},
            {ShaderFlag::MESH, vk::PipelineStageFlagBits::eMeshShaderEXT},
            {ShaderFlag::RAYGEN, vk::PipelineStageFlagBits::eRayTracingShaderKHR},
            {ShaderFlag::ANY_HIT, vk::PipelineStageFlagBits::eRayTracingShaderKHR},
            {ShaderFlag::CLOSEST_HIT, vk::PipelineStageFlagBits::eRayTracingShaderKHR},
            {ShaderFlag::MISS, vk::PipelineStageFlagBits::eRayTracingShaderKHR},
            {ShaderFlag::INTERSECTION, vk::PipelineStageFlagBits::eRayTracingShaderKHR},
            {ShaderFlag::CALLABLE, vk::PipelineStageFlagBits::eRayTracingShaderKHR},
        }};

        std::uint32_t shader_read_pipeline_stages(ShaderFlag stages) {
            if (!detail::valid_shader_read_stages(stages))
                throw std::invalid_argument("Shader read stages must be a nonempty set of known shader stages");
            vk::PipelineStageFlags result;
            if (contain(stages, ShaderFlag::ALL_GRAPHICS)) {
                result |= vk::PipelineStageFlagBits::eAllGraphics;
                stages ^= ShaderFlag::ALL_GRAPHICS;
            }
            for (auto [shader, pipeline] : SHADER_READ_STAGES) {
                if (contain(stages, shader))
                    result |= pipeline;
            }
            return static_cast<std::uint32_t>(result);
        }
    } // namespace
} // namespace VSL_NAMESPACE::command

VSL_NAMESPACE::command::ChangeImageBarrier::ChangeImageBarrier(VSL_NAMESPACE::ImageAccessor image,
                                                               ImageLayout old_layout,
                                                               ImageLayout new_layout)
    : ChangeImageBarrier(std::move(image), old_layout, new_layout, 0, 0) {}

VSL_NAMESPACE::command::ChangeImageBarrier::ChangeImageBarrier(VSL_NAMESPACE::ImageAccessor image, ImageLayout old_layout,
                                                               ImageLayout new_layout, ShaderFlag shader_read_stages)
    : ChangeImageBarrier(std::move(image), old_layout, new_layout, 0, 0, shader_read_stages) {}

VSL_NAMESPACE::command::ChangeImageBarrier::ChangeImageBarrier(VSL_NAMESPACE::ImageAccessor image, ImageLayout old_layout,
                                                               ImageLayout new_layout, std::uint32_t base_layer,
                                                               std::uint32_t layer_count)
    : old_layout(old_layout), new_layout(new_layout), image(std::move(image)), base_layer(base_layer), layer_count(layer_count),
      _shader_read_pipeline_stages(static_cast<std::uint32_t>(vk::PipelineStageFlagBits::eAllCommands)) {}

VSL_NAMESPACE::command::ChangeImageBarrier::ChangeImageBarrier(VSL_NAMESPACE::ImageAccessor image, ImageLayout old_layout,
                                                               ImageLayout new_layout, std::uint32_t base_layer,
                                                               std::uint32_t layer_count, ShaderFlag shader_read_stages)
    : ChangeImageBarrier(std::move(image), old_layout, new_layout, base_layer, layer_count) {
    if (old_layout != ImageLayout::TRANSFER_DST_OPTIMAL || new_layout != ImageLayout::SHADER_READ_ONLY_OPTIMAL)
        throw std::invalid_argument("Shader read stages require a transfer-destination to shader-read transition");
    _shader_read_pipeline_stages = shader_read_pipeline_stages(shader_read_stages);
    _shader_read_stages = shader_read_stages;
}

void VSL_NAMESPACE::command::ChangeImageBarrier::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer,
                                                        VSL_NAMESPACE::CommandManager manager) {
    if (_shader_read_stages != ShaderFlag::ERROR_ &&
        (old_layout != ImageLayout::TRANSFER_DST_OPTIMAL || new_layout != ImageLayout::SHADER_READ_ONLY_OPTIMAL))
        throw std::invalid_argument("Shader read stages require a transfer-destination to shader-read transition");
    if (!image.data_ || !image.data_->image || !buffer.data_ || !buffer.data_->command_pool || !manager.data_ ||
        image.data_->device != manager.data_->device ||
        buffer.data_->command_pool->device != image.data_->device)
        throw std::invalid_argument("Image barrier requires an image and command buffer on the same device");
    if (base_layer >= image.data_->count)
        throw std::out_of_range("Image barrier base layer exceeds image");
    const auto layers = layer_count ? layer_count : image.data_->count - base_layer;
    if (layers > image.data_->count - base_layer)
        throw std::out_of_range("Image barrier layer range exceeds image");
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = (vk::ImageLayout)old_layout;
    barrier.newLayout = (vk::ImageLayout)new_layout;
    barrier.image = image.data_->image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = impl::helper::image_aspects(image.data_->format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = base_layer;
    barrier.subresourceRange.layerCount = layers;

    vk::PipelineStageFlags source_stage, destination_stage;
    if (old_layout == ImageLayout::UNDEFINED && new_layout == ImageLayout::TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL && new_layout == ImageLayout::TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        source_stage = vk::PipelineStageFlagBits::eAllCommands;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == ImageLayout::UNDEFINED && new_layout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else if (old_layout == ImageLayout::TRANSFER_DST_OPTIMAL && new_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlags(_shader_read_pipeline_stages);
    } else if (old_layout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL && new_layout == ImageLayout::TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        source_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == ImageLayout::TRANSFER_SRC_OPTIMAL && new_layout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else if (new_layout == ImageLayout::TRANSFER_SRC_OPTIMAL &&
               (old_layout == ImageLayout::GENERAL || old_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL ||
                old_layout == ImageLayout::TRANSFER_DST_OPTIMAL || old_layout == ImageLayout::PRESENT ||
                old_layout == ImageLayout::TRANSFER_SRC_OPTIMAL)) {
        barrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        source_stage = vk::PipelineStageFlagBits::eAllCommands;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == ImageLayout::TRANSFER_SRC_OPTIMAL &&
               (new_layout == ImageLayout::GENERAL || new_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL ||
                new_layout == ImageLayout::TRANSFER_DST_OPTIMAL || new_layout == ImageLayout::PRESENT)) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eAllCommands;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    buffer.retain(image.data_);
    buffer.data_->command_buffers.at(buffer.get_current_buffer_idx()).pipelineBarrier(source_stage, destination_stage, {}, nullptr, nullptr, barrier);
}
