#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>
#include <utility>

const VSL_NAMESPACE::impl::PipelineRecipe& VSL_NAMESPACE::PipelineLayoutAccessor::recipe() const& {
    if (!_recipe)
        throw std::logic_error("Pipeline layout has no regeneration recipe");
    return *_recipe;
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::PipelineLayoutAccessor::get_device() const {
    if (!data_ || !data_.use_count() || !data_->device || !data_->pipeline_layout)
        throw std::invalid_argument("Pipeline layout requires a live runtime owner");
    return {data_->device->shared_from_this()};
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::PipelineLayoutAccessor::owner_ref() const {
    if (!data_ || !data_.use_count())
        throw std::invalid_argument("Pipeline layout requires an owner");
    return lifetime::detail::owned_ref(data_, data_->owner_node);
}
VSL_NAMESPACE::impl::PipelineRecipe& VSL_NAMESPACE::PipelineLayoutEditAccessor::edit_recipe() const& {
    if (!_edit_recipe)
        throw std::logic_error("Pipeline layout has been consumed");
    return *_edit_recipe;
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::PipelineLayoutEditAccessor::get_device() const {
    return _edit_device.data_ ? _edit_device : PipelineLayoutAccessor::get_device();
}
void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_start(LogicalDeviceAccessor device) {
    if (!device.data_ || !device.data_.use_count())
        throw std::invalid_argument("PipelineLayout requires a device owner");
    operation::validate_pipeline_device(*device.data_);
    _edit_recipe = std::make_shared<impl::PipelineRecipe>();
    _recipe = _edit_recipe;
    _edit_device = std::move(device);
}
void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_finish(PipelineLayoutAccessor runtime) {
    const auto& info = edit_recipe();
    if (runtime.data_) {
        (void)runtime.get_device();
        if (runtime.data_->device != _edit_device.data_.get() ||
            !operation::pipeline_layout_matches(*runtime.data_, info.binding_layouts, info.push_ranges))
            throw std::invalid_argument("Pipeline recipe does not match the runtime layout");
        data_ = std::move(runtime.data_);
    } else {
        data_ = lifetime::make_heap_pipeline_layout(_edit_device, info.binding_layouts, info.push_ranges);
    }
    _edit_device.data_.reset();
}
void VSL_NAMESPACE::PipelineLayoutEditAccessor::swap_state(PipelineLayoutEditAccessor& other) noexcept {
    PipelineLayoutAccessor::swap_state(other);
    _edit_recipe.swap(other._edit_recipe);
    _edit_device.data_.swap(other._edit_device.data_);
}
VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::clone_state() const {
    if (!data_ || !_recipe || !_edit_recipe)
        throw std::logic_error("PipelineLayout has been consumed");
    PipelineLayout result;
    result._edit_device = get_device();
    result._edit_recipe = std::make_shared<impl::PipelineRecipe>(*_edit_recipe);
    result._recipe = result._edit_recipe;
    return result;
}
VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::copy() const {
    auto result = clone_state();
    result.init_finish();
    return result;
}
VSL_NAMESPACE::FrozenPipelineLayout VSL_NAMESPACE::PipelineLayout::freeze() && {
    if (!data_ || !_recipe || !_edit_recipe)
        throw std::logic_error("PipelineLayout has been consumed");
    PipelineLayoutAccessor result(data_);
    result._recipe = _edit_recipe.use_count() == 2 ? _recipe : std::make_shared<impl::PipelineRecipe>(*_edit_recipe);
    PipelineLayout previous;
    swap_state(previous);
    return FrozenPipelineLayout(std::move(result));
}
