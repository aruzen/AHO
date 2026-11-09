#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

void VSL_NAMESPACE::GraphicsPipelineState::injection(PipelineLayoutEditAccessor layout) const {
    static_assert(std::is_nothrow_swappable_v<impl::GraphicsPipelineStateData>);
    auto next = definition();
    std::swap(layout.edit_state().get(), next);
}

const VSL_NAMESPACE::impl::PipelineRecipe& VSL_NAMESPACE::PipelineLayoutAccessor::recipe() const& {
    if (!_recipe)
        throw std::logic_error("Pipeline layout has no regeneration recipe");
    return *_recipe;
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::PipelineLayoutAccessor::get_device() const {
    if (!_owner || !_owner.use_count() || !_owner->device || !_owner->pipeline_layout)
        throw std::invalid_argument("Pipeline layout requires a live runtime owner");
    return lifetime::device_from_child(_owner);
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::PipelineLayoutAccessor::owner_ref() const {
    if (!_owner || !_owner.use_count())
        throw std::invalid_argument("Pipeline layout requires an owner");
    return _owner;
}
VSL_NAMESPACE::impl::PipelineRecipe& VSL_NAMESPACE::PipelineLayoutEditAccessor::edit_recipe() const& {
    if (!_edit_recipe)
        throw std::logic_error("Pipeline layout has been consumed");
    return *_edit_recipe;
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::PipelineLayoutEditAccessor::get_device() const {
    return _edit_device.data_ ? _edit_device : _value.get_device();
}
VSL_NAMESPACE::GraphicsPipelineStateEditRef VSL_NAMESPACE::PipelineLayoutEditAccessor::edit_state() const& {
    return {&edit_recipe().graphics_state};
}
void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_start(LogicalDeviceAccessor device) {
    lifetime::require_device_owner(device);
    operation::validate_pipeline_device(*device.data_);
    _edit_recipe = std::make_shared<impl::PipelineRecipe>();
    _value._recipe = _edit_recipe;
    _edit_device = std::move(device);
}
void VSL_NAMESPACE::PipelineLayoutEditAccessor::init_finish(PipelineLayoutAccessor runtime) {
    if (const auto& pending = edit_recipe().pending_layouts) {
        std::vector<graphic_resource::BindingLayout> layouts;
        layouts.reserve(pending->size());
        for (const auto& info : *pending)
            layouts.emplace_back(_edit_device, info.bindings);
        lifetime::replace_recipe_layouts(edit_recipe(), _edit_device, layouts);
    }
    const auto& info = edit_recipe();
    if (GraphicsAccess::Owned(runtime)) {
        (void)runtime.get_device();
        if (GraphicsAccess::Owned(runtime)->device != _edit_device.data_.get() ||
            !operation::pipeline_layout_matches(*GraphicsAccess::Owned(runtime), info.binding_layouts, info.push_ranges))
            throw std::invalid_argument("Pipeline recipe does not match the runtime layout");
        GraphicsAccess::Owned(*this) = std::move(GraphicsAccess::Owned(runtime));
    } else {
        GraphicsAccess::Owned(*this) = GraphicsAccess::Owned(GraphicsAccess::FromShared(lifetime::make_heap_pipeline_layout(_edit_device, info.binding_layouts, info.push_ranges)));
    }
    _edit_device.data_.reset();
}
void VSL_NAMESPACE::PipelineLayoutEditAccessor::swap_state(PipelineLayoutEditAccessor& other) noexcept {
    _value.swap(other._value);
    _edit_recipe.swap(other._edit_recipe);
    _edit_device.data_.swap(other._edit_device.data_);
}
VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::clone_state() const {
    if (!GraphicsAccess::Owned(*this) || !_value._recipe || !_edit_recipe)
        throw std::logic_error("PipelineLayout has been consumed");
    PipelineLayout result;
    result._edit_device = get_device();
    result._edit_recipe = std::make_shared<impl::PipelineRecipe>(*_edit_recipe);
    result._value._recipe = result._edit_recipe;
    return result;
}
VSL_NAMESPACE::PipelineLayout VSL_NAMESPACE::PipelineLayout::copy() const {
    auto result = clone_state();
    result.init_finish();
    return result;
}
VSL_NAMESPACE::FrozenPipelineLayout VSL_NAMESPACE::PipelineLayout::freeze() && {
    if (!GraphicsAccess::Owned(*this) || !_value._recipe || !_edit_recipe)
        throw std::logic_error("PipelineLayout has been consumed");
    PipelineLayoutAccessor result(GraphicsAccess::Owned(*this));
    result._recipe = _edit_recipe.use_count() == 2 ? _value._recipe : std::make_shared<impl::PipelineRecipe>(*_edit_recipe);
    PipelineLayout previous;
    swap_state(previous);
    return FrozenPipelineLayout(std::move(result));
}

VSL_NAMESPACE::PipelineLayoutAccessor VSL_NAMESPACE::lifetime::detail::GraphicsAccess::FromShared(std::shared_ptr<impl::PipelineLayoutImpl> owner) {
    const auto* node = owner ? owner->owner_node : nullptr;
    return FromOwned(owned_ref(std::move(owner), node));
}
