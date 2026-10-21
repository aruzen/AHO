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

template<VSL_NAMESPACE::ShaderType type>
VSL_NAMESPACE::Shader<type>::Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path) {
    data_ = std::make_shared<impl::ShaderImpl>();
    data_->device = device.data_;
    const auto code = read_shader_code(path);

    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = code.size() * sizeof(std::uint32_t);
    create_info.pCode = code.data();

    data_->shader_module = data_->device->device.createShaderModule(create_info);

    data_->name = "main";

    data_->shader_stage_create_info.pName = data_->name.c_str();
    data_->shader_stage_create_info.module = data_->shader_module;
    data_->shader_stage_create_info.stage = (vk::ShaderStageFlagBits)type;
}

template<VSL_NAMESPACE::ShaderType type>
VSL_NAMESPACE::Shader<type>::Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path, std::string name) {
    data_ = std::make_shared<impl::ShaderImpl>();
    data_->device = device.data_;
    const auto code = read_shader_code(path);

    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = code.size() * sizeof(std::uint32_t);
    create_info.pCode = code.data();

    data_->shader_module = data_->device->device.createShaderModule(create_info);

    data_->name = std::move(name);

    data_->shader_stage_create_info.pName = data_->name.c_str();
    data_->shader_stage_create_info.module = data_->shader_module;
    data_->shader_stage_create_info.stage = (vk::ShaderStageFlagBits)type;
}

template<VSL_NAMESPACE::ShaderType type>
std::string VSL_NAMESPACE::Shader<type>::name() const {
    return data_->name;
}

VSL_NAMESPACE::impl::ShaderImpl::~ShaderImpl() {
    device->device.destroyShaderModule(shader_module);
}

template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::ERROR_>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::VERTEX>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::TESSELLATION_CONTROL>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::TESSELLATION_EVALUATION>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::GEOMETRY>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::FRAGMENT>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::COMPUTE>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::ALL_GRAPHICS>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::RAYGEN>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::ANY_HIT>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::CLOSEST_HIT>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::MISS>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::INTERSECTION>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::CALLABLE>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::TASK>;
template struct VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::MESH>;
// template struct vsl::Shader<VSL_NAMESPACE::ShaderType::All>;
