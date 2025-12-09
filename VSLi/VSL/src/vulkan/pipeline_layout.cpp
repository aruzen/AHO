#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif
#include <VSL/define.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include <string>

void VSL_NAMESPACE::PipelineLayoutAccessor::init_start(LogicalDeviceAccessor device) {
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl>(new VSL_NAMESPACE::_impl::PipelineLayout_impl);
	_data->info = std::shared_ptr<VSL_NAMESPACE::_impl::CreateInfo>(new VSL_NAMESPACE::_impl::CreateInfo);
	_data->device = device._data;

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
	_data->info->_viewport.scissorCount = (uint32_t)_data->info->scissors.size();

	auto& enabledDynamicStates = _data->info->enabledDynamicStates;
	if (_data->info->_viewport.viewportCount != 0)
		_data->info->_viewport.pViewports = _data->info->viewports.data();
	else if (std::find(enabledDynamicStates.begin(), enabledDynamicStates.end(), vk::DynamicState::eViewportWithCount) == enabledDynamicStates.end())
		enabledDynamicStates.push_back(vk::DynamicState::eViewportWithCount);

	if (_data->info->_viewport.scissorCount != 0)
		_data->info->_viewport.pScissors = _data->info->scissors.data();
	else if (std::find(enabledDynamicStates.begin(), enabledDynamicStates.end(), vk::DynamicState::eScissorWithCount) == enabledDynamicStates.end())
		enabledDynamicStates.push_back(vk::DynamicState::eScissorWithCount);

	_data->info->dynamicState.dynamicStateCount = static_cast<uint32_t>(enabledDynamicStates.size());
	_data->info->dynamicState.pDynamicStates = enabledDynamicStates.data();

	_data->pipelineLayout = _data->device->device.createPipelineLayout(_data->info->pipelineLayout);
}

VSL_NAMESPACE::PipelineLayout::PipelineLayout(std::shared_ptr<_impl::PipelineLayout_impl> _data)
	: PipelineLayoutAccessor{ _data } {}

VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::copy() {
	auto data = std::shared_ptr<VSL_NAMESPACE::_impl::PipelineLayout_impl>(new VSL_NAMESPACE::_impl::PipelineLayout_impl(*_data));
	data->info = std::shared_ptr<VSL_NAMESPACE::_impl::CreateInfo>(new VSL_NAMESPACE::_impl::CreateInfo(*_data->info));
	init_finish();
	return PipelineLayout(data);
}

vsl::PipelineLayoutAccessor vsl::PipelineLayout::freeze() {
    _data->info.reset();
    return vsl::PipelineLayoutAccessor{_data};
}

VSL_NAMESPACE::_impl::PipelineLayout_impl::~PipelineLayout_impl()
{
	device->device.destroyPipelineLayout(pipelineLayout);
}
