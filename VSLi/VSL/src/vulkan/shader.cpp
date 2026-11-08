#include <VSL/define.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>

#include <VSL/vulkan/shader.hpp>
#include <VSL/lifetime/shader_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>

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

template<VSL_NAMESPACE::ShaderType type_>
VSL_NAMESPACE::Shader<type_>::Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path)
    : Shader(device, path, "main") {}

template<VSL_NAMESPACE::ShaderType type_>
VSL_NAMESPACE::Shader<type_>::Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::filesystem::path path, std::string name)
    : ShaderAccessor{lifetime::make_heap_shader(device, {type_, read_shader_code(path), std::move(name)})} {}

template<VSL_NAMESPACE::ShaderType type_>
VSL_NAMESPACE::Shader<type_>::Shader(VSL_NAMESPACE::LogicalDeviceAccessor device, std::span<const std::uint32_t> code, std::string name) {
    data_ = lifetime::make_heap_shader(device, {type_, {code.begin(), code.end()}, std::move(name)});
}

template<VSL_NAMESPACE::ShaderType type_>
std::string VSL_NAMESPACE::Shader<type_>::name() const {
    return ShaderAccessor::name();
}

std::string VSL_NAMESPACE::ShaderAccessor::name() const {
    if (!data_)
        throw std::logic_error("Shader is empty");
    return data_->name;
}
VSL_NAMESPACE::ShaderType VSL_NAMESPACE::ShaderAccessor::type() const {
    if (!data_)
        throw std::logic_error("Shader is empty");
    return static_cast<ShaderType>(data_->stage);
}
std::span<const std::uint32_t> VSL_NAMESPACE::ShaderAccessor::code() const& {
    if (!data_)
        throw std::logic_error("Shader is empty");
    return data_->code;
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::ShaderAccessor::get_device() const {
    if (!data_ || !data_->device)
        throw std::logic_error("Shader is empty");
    return {data_->device->shared_from_this()};
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::ShaderAccessor::owner_ref() const {
    return lifetime::detail::owned_ref(data_, data_ ? data_->owner_node : nullptr);
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
