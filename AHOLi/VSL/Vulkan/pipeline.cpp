#include "pch.h"
#include "../define.h"
#include "pipeline.h"

#include "_pimpls.h"

#include <string>

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

VSL_NAMESPACE::PipelineStage<VSL_NAMESPACE::PipelineStageType::Vertex>::PipelineStage(std::string name, VSL_NAMESPACE::Shader shader)
{
	type = VSL_NAMESPACE::PipelineStageType::Vertex;

	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);
	_info = _data.get();

	_data->stage.stage = vk::ShaderStageFlagBits::eVertex;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

VSL_NAMESPACE::PipelineStage<VSL_NAMESPACE::PipelineStageType::Fragment>::PipelineStage(std::string name, VSL_NAMESPACE::Shader shader)
{
	type = VSL_NAMESPACE::PipelineStageType::Fragment;

	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);
	_info = _data.get();

	_data->stage.stage = vk::ShaderStageFlagBits::eFragment;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

template<bool V, VSL_NAMESPACE::PipelineStageType Type>
VSL_NAMESPACE::Pipeline<V>& operator <<(VSL_NAMESPACE::Pipeline<V>& out, const VSL_NAMESPACE::PipelineStage<Type>& stage) {

}

template struct vsl::Pipeline<true>;
template struct vsl::Pipeline<false>;
template struct vsl::PipelineStage<VSL_NAMESPACE::PipelineStageType::Vertex>;
template struct vsl::PipelineStage<VSL_NAMESPACE::PipelineStageType::Fragment>;