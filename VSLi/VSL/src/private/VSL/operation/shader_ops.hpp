#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/shader_create_info.hpp>

namespace VSL_NAMESPACE::operation {
    void initialize_shader(impl::ShaderImpl& target, impl::LogicalDeviceImpl& device, ShaderCreateInfo info);
    void destroy_shader(impl::ShaderImpl& target) noexcept;
    vk::PipelineShaderStageCreateInfo shader_stage(const impl::ShaderImpl& shader);
} // namespace VSL_NAMESPACE::operation
