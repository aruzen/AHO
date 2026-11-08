#include <VSL/operation/shader_ops.hpp>
#include <VSL/exceptions.hpp>
#include <limits>
#include <bit>

namespace VSL_NAMESPACE::operation {
    void initialize_shader(impl::ShaderImpl& target, impl::LogicalDeviceImpl& device, ShaderCreateInfo info) {
        if (info.code.size() < 5 || info.code.front() != 0x07230203 || info.code.size() > std::numeric_limits<size_t>::max() / sizeof(std::uint32_t))
            throw exceptions::RuntimeException("Shader", "Invalid SPIR-V header or word count");
        if (info.name.empty() || info.name.find('\0') != std::string::npos)
            throw exceptions::RuntimeException("Shader", "Shader entry point must be a nonempty name without null bytes");
        if (!device.device)
            throw exceptions::RuntimeException("Shader", "A live logical device is required");
        target.device = &device;
        target.native_device = device.device;
        target.stage = static_cast<vk::ShaderStageFlagBits>(info.stage);
        target.name = std::move(info.name);
        target.code = std::move(info.code);
        const vk::ShaderModuleCreateInfo create{.codeSize = target.code.size() * sizeof(std::uint32_t), .pCode = target.code.data()};
        target.shader_module = device.device.createShaderModule(create);
    }
    void destroy_shader(impl::ShaderImpl& target) noexcept {
        if (target.shader_module)
            target.native_device.destroyShaderModule(target.shader_module);
        target.shader_module = nullptr;
    }
    vk::PipelineShaderStageCreateInfo shader_stage(const impl::ShaderImpl& shader) {
        if (!shader.shader_module || shader.name.empty())
            throw std::invalid_argument("Shader stage requires a live module and entry point");
        const auto stage = static_cast<std::uint32_t>(shader.stage);
        if (!std::has_single_bit(stage) || (stage & ~std::uint32_t{0x3fff}))
            throw std::invalid_argument("Shader stage requires one supported stage bit");
        return {.stage = shader.stage, .module = shader.shader_module, .pName = shader.name.c_str()};
    }
} // namespace VSL_NAMESPACE::operation
