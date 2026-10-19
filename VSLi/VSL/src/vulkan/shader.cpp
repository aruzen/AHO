#include <VSL/define.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>

#include <VSL/vulkan/shader.hpp>

#include <fstream>
#include <cstdint>
#include <limits>
#include <vector>
#include <memory>
#include <source_location>
#include <utility>

namespace {
    std::vector<std::uint32_t> read_shader_code(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw VSL_NAMESPACE::exceptions::RuntimeException("file", "failed to open file: " + path.string());
        const auto byte_count = static_cast<std::streamoff>(file.tellg());
        constexpr std::size_t header_bytes = 5 * sizeof(std::uint32_t);
        if (byte_count < static_cast<std::streamoff>(header_bytes) || byte_count % sizeof(std::uint32_t) != 0 ||
            static_cast<std::uintmax_t>(byte_count) > std::numeric_limits<std::size_t>::max() ||
            static_cast<std::uintmax_t>(byte_count) > std::numeric_limits<std::streamsize>::max())
            throw VSL_NAMESPACE::exceptions::RuntimeException("file", "invalid shader byte size: " + path.string());
        std::vector<std::uint32_t> code(static_cast<std::size_t>(byte_count) / sizeof(std::uint32_t));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(byte_count)))
            throw VSL_NAMESPACE::exceptions::RuntimeException("file", "failed to read shader file: " + path.string());
        return code;
    }
} // namespace

template<VSL_NAMESPACE::ShaderType _Type>
VSL_NAMESPACE::Shader<_Type>::Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path) {
    _data = std::make_shared<_impl::Shader_impl>();
    _data->device = device._data;
    const auto code = read_shader_code(path);

    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize = code.size() * sizeof(std::uint32_t);
    createInfo.pCode = code.data();

    _data->shaderModule = _data->device->device.createShaderModule(createInfo);

    _data->name = "main";

    _data->shaderStageCreateInfo.pName = _data->name.c_str();
    _data->shaderStageCreateInfo.module = _data->shaderModule;
    _data->shaderStageCreateInfo.stage = (vk::ShaderStageFlagBits)_Type;
}

template<VSL_NAMESPACE::ShaderType _Type>
VSL_NAMESPACE::Shader<_Type>::Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path, std::string name) {
    _data = std::make_shared<_impl::Shader_impl>();
    _data->device = device._data;
    const auto code = read_shader_code(path);

    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize = code.size() * sizeof(std::uint32_t);
    createInfo.pCode = code.data();

    _data->shaderModule = _data->device->device.createShaderModule(createInfo);

    _data->name = std::move(name);

    _data->shaderStageCreateInfo.pName = _data->name.c_str();
    _data->shaderStageCreateInfo.module = _data->shaderModule;
    _data->shaderStageCreateInfo.stage = (vk::ShaderStageFlagBits)_Type;
}

template<VSL_NAMESPACE::ShaderType _Type>
std::string VSL_NAMESPACE::Shader<_Type>::name() const {
    return _data->name;
}

VSL_NAMESPACE::_impl::Shader_impl::~Shader_impl() {
    device->device.destroyShaderModule(shaderModule);
}

template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Error>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Vertex>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::TessellationControl>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::TessellationEvaluation>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Geometry>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Fragment>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Compute>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::AllGraphics>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Raygen>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::AnyHit>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::ClosestHit>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Miss>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Intersection>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Callable>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Task>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Mesh>;
// template struct vsl::Shader<VSL_NAMESPACE::ShaderType::All>;
