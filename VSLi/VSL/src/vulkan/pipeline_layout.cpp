#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>
#include <utility>

namespace {
    template<typename T>
    void rebind(const VSL_NAMESPACE::_impl::CreateInfo& info, const char* key, const T*& pointer, std::uint32_t& count) {
        if (auto it = info.pool.find(key); it != info.pool.end()) {
            const auto& values = std::any_cast<const std::vector<T>&>(it->second);
            pointer = values.empty() ? nullptr : values.data();
            count = static_cast<std::uint32_t>(values.size());
        }
    }
    void rebind_storage(VSL_NAMESPACE::_impl::CreateInfo& info) {
        info._viewport.viewportCount = static_cast<std::uint32_t>(info.viewports.size());
        info._viewport.pViewports = info.viewports.empty() ? nullptr : info.viewports.data();
        info._viewport.scissorCount = static_cast<std::uint32_t>(info.scissors.size());
        info._viewport.pScissors = info.scissors.empty() ? nullptr : info.scissors.data();
        info.dynamicState.dynamicStateCount = static_cast<std::uint32_t>(info.enabledDynamicStates.size());
        info.dynamicState.pDynamicStates = info.enabledDynamicStates.empty() ? nullptr : info.enabledDynamicStates.data();
        rebind(info, "DescriptorSetLayouts", info.pipelineLayout.pSetLayouts, info.pipelineLayout.setLayoutCount);
        rebind(info, "colorBlendAttachment", info.pipelineLayout.pPushConstantRanges, info.pipelineLayout.pushConstantRangeCount);
        rebind(info, "vertexBindingDescriptions", info.vertexInput.pVertexBindingDescriptions, info.vertexInput.vertexBindingDescriptionCount);
        rebind(info, "vertexAttributeDescriptions", info.vertexInput.pVertexAttributeDescriptions, info.vertexInput.vertexAttributeDescriptionCount);
        rebind(info, "colorBlendAttachments", info.colorBlend.pAttachments, info.colorBlend.attachmentCount);
    }
} // namespace

void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_start(LogicalDeviceAccessor device) {
    if (!device._data)
        throw std::invalid_argument("PipelineLayout requires a device");
    _data = std::make_shared<_impl::PipelineLayout_impl>();
    _data->device = device._data;
    _data->info = std::make_shared<_impl::CreateInfo>();
}

void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_finish() {
    rebind_storage(*_data->info);
    auto handle = _data->device->device.createPipelineLayout(_data->info->pipelineLayout);
    _data->device->device.destroyPipelineLayout(_data->pipelineLayout);
    _data->pipelineLayout = handle;
}

VSL_NAMESPACE::PipelineLayout::PipelineLayout(std::shared_ptr<_impl::PipelineLayout_impl> data)
    : PipelineLayoutEditAccessor{{std::move(data)}} {}

VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::clone_state() const {
    if (!_data || !_data->info)
        throw std::logic_error("PipelineLayout has been consumed");
    auto data = std::make_shared<_impl::PipelineLayout_impl>();
    data->device = _data->device;
    data->info = std::make_shared<_impl::CreateInfo>(*_data->info);
    return PipelineLayout(std::move(data));
}

VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::copy() const {
    auto result = clone_state();
    result.init_finish();
    return result;
}

VSL_NAMESPACE::FrozenPipelineLayout VSL_NAMESPACE::PipelineLayout::freeze() && {
    if (!_data || !_data->info)
        throw std::logic_error("PipelineLayout has been consumed");
    // 編集用の別名が残る場合も、構築済みの状態を共有編集させない。
    if (_data.use_count() != 1) {
        auto snapshot = copy();
        _data.reset();
        return FrozenPipelineLayout(std::move(snapshot._data));
    }
    return FrozenPipelineLayout(std::exchange(_data, {}));
}

VSL_NAMESPACE::_impl::PipelineLayout_impl::~PipelineLayout_impl() {
    if (device)
        device->device.destroyPipelineLayout(pipelineLayout);
}
