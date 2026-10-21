#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>
#include <utility>

namespace {
    template<typename T>
    void rebind(const VSL_NAMESPACE::impl::CreateInfo& info, const char* key, const T*& pointer, std::uint32_t& count) {
        if (auto it = info.pool.find(key); it != info.pool.end()) {
            const auto& values = std::any_cast<const std::vector<T>&>(it->second);
            pointer = values.empty() ? nullptr : values.data();
            count = static_cast<std::uint32_t>(values.size());
        }
    }
    void rebind_storage(VSL_NAMESPACE::impl::CreateInfo& info) {
        info.viewport.viewportCount = static_cast<std::uint32_t>(info.viewports.size());
        info.viewport.pViewports = info.viewports.empty() ? nullptr : info.viewports.data();
        info.viewport.scissorCount = static_cast<std::uint32_t>(info.scissors.size());
        info.viewport.pScissors = info.scissors.empty() ? nullptr : info.scissors.data();
        info.dynamic_state.dynamicStateCount = static_cast<std::uint32_t>(info.enabled_dynamic_states.size());
        info.dynamic_state.pDynamicStates = info.enabled_dynamic_states.empty() ? nullptr : info.enabled_dynamic_states.data();
        rebind(info, "DescriptorSetLayouts", info.pipeline_layout.pSetLayouts, info.pipeline_layout.setLayoutCount);
        rebind(info, "colorBlendAttachment", info.pipeline_layout.pPushConstantRanges, info.pipeline_layout.pushConstantRangeCount);
        rebind(info, "vertexBindingDescriptions", info.vertex_input.pVertexBindingDescriptions, info.vertex_input.vertexBindingDescriptionCount);
        rebind(info, "vertexAttributeDescriptions", info.vertex_input.pVertexAttributeDescriptions, info.vertex_input.vertexAttributeDescriptionCount);
        rebind(info, "colorBlendAttachments", info.color_blend.pAttachments, info.color_blend.attachmentCount);
    }
} // namespace

void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_start(LogicalDeviceAccessor device) {
    if (!device.data_)
        throw std::invalid_argument("PipelineLayout requires a device");
    data_ = std::make_shared<impl::PipelineLayoutImpl>();
    data_->device = device.data_;
    data_->info = std::make_shared<impl::CreateInfo>();
}

void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_finish() {
    rebind_storage(*data_->info);
    auto handle = data_->device->device.createPipelineLayout(data_->info->pipeline_layout);
    data_->device->device.destroyPipelineLayout(data_->pipeline_layout);
    data_->pipeline_layout = handle;
}

VSL_NAMESPACE::PipelineLayout::PipelineLayout(std::shared_ptr<impl::PipelineLayoutImpl> data)
    : PipelineLayoutEditAccessor{{std::move(data)}} {}

VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::clone_state() const {
    if (!data_ || !data_->info)
        throw std::logic_error("PipelineLayout has been consumed");
    auto data = std::make_shared<impl::PipelineLayoutImpl>();
    data->device = data_->device;
    data->info = std::make_shared<impl::CreateInfo>(*data_->info);
    return PipelineLayout(std::move(data));
}

VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::copy() const {
    auto result = clone_state();
    result.init_finish();
    return result;
}

VSL_NAMESPACE::FrozenPipelineLayout VSL_NAMESPACE::PipelineLayout::freeze() && {
    if (!data_ || !data_->info)
        throw std::logic_error("PipelineLayout has been consumed");
    // 編集用の別名が残る場合も、構築済みの状態を共有編集させない。
    if (data_.use_count() != 1) {
        auto snapshot = copy();
        data_.reset();
        return FrozenPipelineLayout(std::move(snapshot.data_));
    }
    return FrozenPipelineLayout(std::exchange(data_, {}));
}

VSL_NAMESPACE::impl::PipelineLayoutImpl::~PipelineLayoutImpl() {
    if (device)
        device->device.destroyPipelineLayout(pipeline_layout);
}
