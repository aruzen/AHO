#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"
#include "pipeline_layout.h"

#include "_pimpls.h"

#include <string>

const auto DEFAULT_DYNAMIC_STATE = std::array<vk::DynamicState, 2>{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
};		

void VSL_NAMESPACE::PipelineLayoutAccessor::init_start(LogicalDeviceAccessor device) {
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl>(new VSL_NAMESPACE::_impl::PipelineLayout_impl);
	_data->info = std::shared_ptr<VSL_NAMESPACE::_impl::CreateInfo>(new VSL_NAMESPACE::_impl::CreateInfo);
	_data->device = device._data;

	_data->info->dynamicState.dynamicStateCount = static_cast<uint32_t>(DEFAULT_DYNAMIC_STATE.size());
	_data->info->dynamicState.pDynamicStates = DEFAULT_DYNAMIC_STATE.data();
	_data->info->pipelineLayout.setLayoutCount = 0; // Optional
	_data->info->pipelineLayout.pSetLayouts = nullptr; // Optional
	_data->info->pipelineLayout.pushConstantRangeCount = 0; // Optional
	_data->info->pipelineLayout.pPushConstantRanges = nullptr; // Optional
}

namespace VSL_NAMESPACE::helper {
	void expansionPipelineLayoutArgs(VSL_NAMESPACE::PipelineLayoutAccessor& pl) {}
}

void VSL_NAMESPACE::PipelineLayoutAccessor::init_finish() {
	_data->info->_viewport.viewportCount = (uint32_t)_data->info->viewports.size();
	_data->info->_viewport.pViewports = _data->info->viewports.data();
	_data->info->_viewport.scissorCount = (uint32_t)_data->info->scissors.size();
	_data->info->_viewport.pScissors = _data->info->scissors.data();

	_data->pipelineLayout = _data->device->device.createPipelineLayout(_data->info->pipelineLayout);
}

VSL_NAMESPACE::_impl::PipelineLayout_impl::~PipelineLayout_impl()
{
	device->device.destroyPipelineLayout(pipelineLayout);
}

template struct vsl::PipelineLayout<>;
