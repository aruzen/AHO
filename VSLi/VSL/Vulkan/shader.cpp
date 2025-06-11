#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"
#include "_pimpls.h"

#include "../exceptions.h"

#include "shader.h"

#include <fstream>
#include <memory>
#include <source_location>

template<VSL_NAMESPACE::ShaderType _Type>
vsl::Shader<_Type>::Shader(vsl::LogicalDeviceAccessor device, std::filesystem::path path)
{
	_data = std::make_shared<_impl::Shader_impl>();
	_data->device = device._data;
	std::vector<char> code;

	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw vsl::exceptions::RuntimeException("file", "failed to open file!", std::source_location::current());
		}

		size_t fileSize = (size_t)file.tellg();
		code.resize(fileSize);
		file.seekg(0);
		file.read(code.data(), fileSize);
		file.close();
	}

	vk::ShaderModuleCreateInfo createInfo;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	_data->shaderModule = _data->device->device.createShaderModule(createInfo);

	_data->name = path.filename().string();

	_data->shaderStageCreateInfo.pName = _data->name.c_str();
	_data->shaderStageCreateInfo.module = _data->shaderModule;
	if constexpr (_Type == ShaderType::Vertex) {
		_data->shaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
	} else if constexpr (_Type == ShaderType::Fragment) {
		_data->shaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
	}
}

template<VSL_NAMESPACE::ShaderType _Type>
vsl::Shader<_Type>::Shader(vsl::LogicalDeviceAccessor device, std::filesystem::path path, std::string name)
{
	_data = std::make_shared<_impl::Shader_impl>();
	_data->device = device._data;
	std::vector<char> code;

	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw vsl::exceptions::RuntimeException("file", "failed to open file!", std::source_location::current());
		}

		size_t fileSize = (size_t)file.tellg();
		code.resize(fileSize);
		file.seekg(0);
		file.read(code.data(), fileSize);
		file.close();
	}

	vk::ShaderModuleCreateInfo createInfo;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	_data->shaderModule = _data->device->device.createShaderModule(createInfo);

	_data->name = name;

	_data->shaderStageCreateInfo.pName = _data->name.c_str();
	_data->shaderStageCreateInfo.module = _data->shaderModule;
    _data->shaderStageCreateInfo.stage = (vk::ShaderStageFlagBits)_Type;
}

VSL_NAMESPACE::_impl::Shader_impl::~Shader_impl()
{
	device->device.destroyShaderModule(shaderModule);
}

template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Error>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Vertex>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::TessellationControl>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::TessellationEvaluation>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Geometry>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Fragment>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Compute>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::AllGraphics>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Raygen>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::AnyHit>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::ClosestHit>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Miss>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Intersection>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Callable>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Task>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Mesh>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::SubpassShadingHuawei>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::ClusterCullingHuawei>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::All>;
