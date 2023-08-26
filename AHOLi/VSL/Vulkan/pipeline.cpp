#include "pch.h"
#include "../define.h"
#include "pipeline.h"

#include "_pimpls.h"

#include <string>

const auto DEFAULT_DYNAMIC_STATE = std::array<vk::DynamicState, 2>{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
};

template<typename T, typename... Args>
void expansionPipelineArgs(std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl> _data, const T& t, const Args& ...args) {
	if constexpr () {

	}

	expansionPipelineArgs(_data, args...);
}

void expansionPipelineArgs(std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl> _data) {}

template<typename... Args>
VSL_NAMESPACE::Pipeline<Args...>::Pipeline(const Args& ...args) {
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::Pipeline_impl>(new VSL_NAMESPACE::_impl::Pipeline_impl);

	// _data->defaultCreateInfo.dynamicStateCount = static_cast<uint32_t>(DEFAULT_DYNAMIC_STATE.size());
	// _data->defaultCreateInfo.pDynamicStates = DEFAULT_DYNAMIC_STATE.data();
}

VSL_NAMESPACE::_impl::Pipeline_impl::~Pipeline_impl()
{

}

VSL_NAMESPACE::ShaderPipelineStage<VSL_NAMESPACE::ShaderPipelineStageType::Vertex>::ShaderPipelineStage(std::string name, VSL_NAMESPACE::Shader shader)
{
	type = VSL_NAMESPACE::ShaderPipelineStageType::Vertex;

	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);

	_data->stage.stage = vk::ShaderStageFlagBits::eVertex;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

VSL_NAMESPACE::ShaderPipelineStage<VSL_NAMESPACE::ShaderPipelineStageType::Fragment>::ShaderPipelineStage(std::string name, VSL_NAMESPACE::Shader shader)
{
	type = VSL_NAMESPACE::ShaderPipelineStageType::Fragment;

	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);

	_data->stage.stage = vk::ShaderStageFlagBits::eFragment;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

template<VSL_NAMESPACE::ShaderPipelineStageType Type, typename... Args>
VSL_NAMESPACE::Pipeline<Args...>& operator <<(VSL_NAMESPACE::Pipeline<Args...>& out, const VSL_NAMESPACE::ShaderPipelineStage<Type>& stage) {

}

template struct vsl::Pipeline<>;
template struct vsl::ShaderPipelineStage<VSL_NAMESPACE::ShaderPipelineStageType::Vertex>;
template struct vsl::ShaderPipelineStage<VSL_NAMESPACE::ShaderPipelineStageType::Fragment>;