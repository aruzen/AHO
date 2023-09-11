#include "pch.h"
#include "../define.h"
#include "pipeline_layout.h"

#include "_pimpls.h"

#include <string>

const auto DEFAULT_DYNAMIC_STATE = std::array<vk::DynamicState, 2>{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
};

template<VSL_NAMESPACE::pipeline_layout_injecter T, VSL_NAMESPACE::pipeline_layout_injecter... Args>
void expansionPipelineLayoutArgs(std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl> _data, VSL_NAMESPACE::_impl::CreateInfo& info, const T& t, const Args& ...args) {
	if constexpr (VSL_NAMESPACE::pipeline_layout_data_injecter<T>) {
		t.injection(_data);
	}
	if constexpr (VSL_NAMESPACE::pipeline_layout_createinfo_injecter<T>) {
		t.injection(info);
	}
	expansionPipelineLayoutArgs(_data, info, args...);
}

void expansionPipelineLayoutArgs(std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl> _data, VSL_NAMESPACE::_impl::CreateInfo& info) {}

template<VSL_NAMESPACE::pipeline_layout_injecter... Args>
VSL_NAMESPACE::PipelineLayout<Args...>::PipelineLayout(VSL_NAMESPACE::LogicalDeviceAcsessor device, const Args& ...args) {
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl>(new VSL_NAMESPACE::_impl::PipelineLayout_impl);
	_data->info = std::shared_ptr<VSL_NAMESPACE::_impl::CreateInfo>(new VSL_NAMESPACE::_impl::CreateInfo);
	_data->device = device._data;

	_data->info->dynamicState.dynamicStateCount = static_cast<uint32_t>(DEFAULT_DYNAMIC_STATE.size());
	_data->info->dynamicState.pDynamicStates = DEFAULT_DYNAMIC_STATE.data();
	_data->info->pipelineLayout.setLayoutCount = 0; // Optional
	_data->info->pipelineLayout.pSetLayouts = nullptr; // Optional
	_data->info->pipelineLayout.pushConstantRangeCount = 0; // Optional
	_data->info->pipelineLayout.pPushConstantRanges = nullptr; // Optional

	expansionPipelineLayoutArgs(_data, *_data->info, args...);

	_data->info->_viewport.viewportCount = _data->info->viewports.size();
	_data->info->_viewport.pViewports = _data->info->viewports.data();
	_data->info->_viewport.scissorCount = _data->info->scissors.size();
	_data->info->_viewport.pScissors = _data->info->scissors.data();

	_data->pipelineLayout = device._data->device.createPipelineLayout(_data->info->pipelineLayout);
}

VSL_NAMESPACE::_impl::PipelineLayout_impl::~PipelineLayout_impl()
{
	device->device.destroyPipelineLayout(pipelineLayout);
}

VSL_NAMESPACE::ShaderPipelineLayoutStage<VSL_NAMESPACE::ShaderPipelineLayoutStageType::Vertex>::ShaderPipelineLayoutStage(std::string name, VSL_NAMESPACE::Shader shader)
{
	type = VSL_NAMESPACE::ShaderPipelineLayoutStageType::Vertex;

	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);

	_data->stage.stage = vk::ShaderStageFlagBits::eVertex;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

VSL_NAMESPACE::ShaderPipelineLayoutStage<VSL_NAMESPACE::ShaderPipelineLayoutStageType::Fragment>::ShaderPipelineLayoutStage(std::string name, VSL_NAMESPACE::Shader shader)
{
	type = VSL_NAMESPACE::ShaderPipelineLayoutStageType::Fragment;

	_data = std::shared_ptr<VSL_NAMESPACE::_impl::ShaderStage_impl>(new VSL_NAMESPACE::_impl::ShaderStage_impl);

	_data->stage.stage = vk::ShaderStageFlagBits::eFragment;
	_data->stage.pName = name.c_str();
	_data->stage.module = shader._data->shaderModule;
}

template struct vsl::PipelineLayout<>;
template struct vsl::ShaderPipelineLayoutStage<VSL_NAMESPACE::ShaderPipelineLayoutStageType::Vertex>;
template struct vsl::ShaderPipelineLayoutStage<VSL_NAMESPACE::ShaderPipelineLayoutStageType::Fragment>;