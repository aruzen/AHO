#include "pch.h"
#include "../define.h"
#include "pipeline.h"

#include "_pimpls.h"

const auto DEFAULT_DYNAMIC_STATE = std::array<vk::DynamicState, 2>{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
};

template<bool V>
VSL_NAMESPACE::Pipeline<V>::Pipeline() {
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl>(new VSL_NAMESPACE::_impl::Pipeline_impl);

	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(DEFAULT_DYNAMIC_STATE.size());
	dynamicState.pDynamicStates = DEFAULT_DYNAMIC_STATE.data();
}

template<bool V>
template<VSL_NAMESPACE::StageID ID, bool _V>
VSL_NAMESPACE::Pipeline<V>& VSL_NAMESPACE::Pipeline<V>::operator<<(VSL_NAMESPACE::Pipeline<V>::Stage<ID, _V> stage)
{
	// vk::Pipeline
	stage._data->stage;
	return *this;
}

template<bool V>
template<bool _V>
VSL_NAMESPACE::Pipeline<V>::Stage<VSL_NAMESPACE::StageID::Vertex, _V>::Stage(std::string name, VSL_NAMESPACE::Shader<V> shader)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);

	_data->stage.stage = vk::ShaderStageFlagBits::eVertex;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

template<bool V>
template<bool _V>
VSL_NAMESPACE::Pipeline<V>::Stage<VSL_NAMESPACE::StageID::Fragment, _V>::Stage(std::string name, VSL_NAMESPACE::Shader<V> shader)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);

	_data->stage.stage = vk::ShaderStageFlagBits::eFragment;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

template struct vsl::Pipeline<true>;
template struct vsl::Pipeline<true>::Stage<VSL_NAMESPACE::StageID::Vertex, true>;
template struct vsl::Pipeline<true>::Stage<VSL_NAMESPACE::StageID::Fragment, true>;