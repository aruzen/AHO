#ifdef _MSC_VER
#include "pch.h"
#endif
#include "shader_group.h"
#include "../_pimpls.h"
#include "../pipeline_layout.h"

VSL_NAMESPACE::pipeline_layout::ShaderGroup::ShaderGroup(std::string name, std::initializer_list<ShaderAccessor> shaders) {
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

void VSL_NAMESPACE::pipeline_layout::ShaderGroup::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	for (auto& itr : _data->shaders)
		info.shaderStages.push_back(itr->shaderStageCreateInfo);
}

