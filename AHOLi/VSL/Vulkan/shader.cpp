#include "pch.h"
#include "../define.h"
#include "_pimpls.h"

#include "../exceptions.h"

#include "shader.h"

#include <fstream>
#include <source_location>

vsl::Shader::Shader(vsl::LogicalDeviceAcsessor device, std::filesystem::path path)
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
}

VSL_NAMESPACE::_impl::Shader_impl::~Shader_impl()
{
	device->device.destroyShaderModule(shaderModule);
}