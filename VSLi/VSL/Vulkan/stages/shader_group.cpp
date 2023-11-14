#include "pch.h"
#include "shader_group.h"
#include "../_pimpls.h"

VSL_NAMESPACE::pipeline_layout::ShaderGroup::ShaderGroup(std::initializer_list<ShaderAccessor> shaders, std::string name) {
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::pipeline_layout::ShaderGroup_impl>(new VSL_NAMESPACE::_impl::pipeline_layout::ShaderGroup_impl);
	_data->name = name;
	for (auto& shader : shaders) {
		_data->shaders.push_back(shader._data);
	}
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::addShader(ShaderAccessor shader) {
	_data->shaders.push_back(shader._data);
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::removeShader(std::string name) {
	for (auto itr = _data->shaders.begin(); itr == _data->shaders.end(); itr++) {
		if ((*itr)->name == name)
			_data->shaders.erase(itr);
	}
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::removeShader(ShaderAccessor shader) {
	for (auto itr = _data->shaders.begin(); itr == _data->shaders.end(); itr++) {
		if ((*itr).get() == shader._data.get())
			_data->shaders.erase(itr);
	}
}

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::injection(VSL_NAMESPACE::_impl::CreateInfo& info)
{
	for (auto& itr : _data->shaders)
		info.shaderStages.push_back(itr->shaderStageCreateInfo);
}