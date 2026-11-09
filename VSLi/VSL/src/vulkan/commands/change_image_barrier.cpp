#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/operation/image_barrier.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/resource_use.hpp>

#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include "../image_format.hpp"
#include <array>
#include <utility>

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
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

void VSL_NAMESPACE::command::ChangeImageBarrier::invoke(CommandPool, CommandBuffer buffer, CommandManager manager) {
    const auto change = *this;
    if (change._shader_read_stages != ShaderFlag::ERROR_ &&
        (change.old_layout != ImageLayout::TRANSFER_DST_OPTIMAL || change.new_layout != ImageLayout::SHADER_READ_ONLY_OPTIMAL))
        throw std::invalid_argument("Shader read stages require a transfer-destination to shader-read transition");
    const auto& image = ImageAccess::Owned(change.image);
    if (!image || !CommandAccess::Data(buffer) || !CommandAccess::Data(buffer)->command_pool || !CommandAccess::Data(manager) ||
        image->device != CommandAccess::Data(manager)->device.get() || CommandAccess::Data(buffer)->command_pool->device != image->device)
        throw std::invalid_argument("Image barrier requires an image and command buffer on the same device");
    const auto barrier = operation::prepare_image_barrier(*image, change.old_layout, change.new_layout, change.base_layer, change.layer_count, change._shader_read_pipeline_stages);
    const NativeRecordingContext recording(buffer);
    recording.use(change.image.resource_use(), ResourceRetention::SUBMISSION);
    operation::record_image_barrier(vk::CommandBuffer(recording.native_command_buffer()), barrier);
}
