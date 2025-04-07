#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"
#include "_pimpls.h"

#include "../exceptions.h"

#include "shader.h"

#include <fstream>
#include <source_location>

template<VSL_NAMESPACE::ShaderType _Type>
vsl::Shader<_Type>::Shader(vsl::LogicalDeviceAccessor device, std::filesystem::path path)
{
	_data = std::shared_ptr<_impl::Shader_impl>(new _impl::Shader_impl);
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

	_data->shaderStageCreateInfo.pName = "main";
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
	_data = std::shared_ptr<_impl::Shader_impl>(new _impl::Shader_impl);
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

	_data->shaderStageCreateInfo.pName = "main";
	_data->shaderStageCreateInfo.module = _data->shaderModule;
	if constexpr (_Type == ShaderType::Vertex) {
		_data->shaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
	}
	else if constexpr (_Type == ShaderType::Fragment) {
		_data->shaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
	}
}

VSL_NAMESPACE::_impl::Shader_impl::~Shader_impl()
{
	device->device.destroyShaderModule(shaderModule);
}

template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Vertex>;
template struct vsl::Shader<VSL_NAMESPACE::ShaderType::Fragment>;
