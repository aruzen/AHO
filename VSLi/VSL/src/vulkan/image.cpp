#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/resource_use_factory.hpp>
#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/image.hpp>

#include <utility>
#include <stdexcept>
#include "image_format.hpp"
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>

size_t VSL_NAMESPACE::ImageAccessor::size() {
    if (!_owner)
        throw std::invalid_argument("Image is empty");
    return std::size_t(_owner->width) * _owner->height * _owner->count * convert((data_format::FormatTypes)_owner->format).size();
}

VSL_NAMESPACE::ImageAccessor::ImageAccessor(LogicalDeviceAccessor device, const ImageCreateInfo& info)
    : ImageAccessor(lifetime::detail::ImageAccess::FromShared(lifetime::make_heap_image(std::move(device), info))) {}

VSL_NAMESPACE::ImageAccessor VSL_NAMESPACE::ImageAccessor::Create(LogicalDeviceAccessor device, const ImageCreateInfo& info) {
    return ImageAccessor(std::move(device), info);
}

VSL_NAMESPACE::ImageAccessor VSL_NAMESPACE::lifetime::detail::ImageAccess::FromShared(std::shared_ptr<impl::ImageImpl> owner) {
    const auto* node = owner ? owner->owner_node : nullptr;
    return FromOwned(owned_ref(std::move(owner), node));
}

void VSL_NAMESPACE::ImageAccessor::TransformLayout(VSL_NAMESPACE::CommandManager manager, ImageAccessor image,
                                                   VSL_NAMESPACE::ImageLayout old_layout, VSL_NAMESPACE::ImageLayout new_layout) {
    auto phase = manager.start_phase<ComputePhase>();
    phase << command::ChangeImageBarrier(std::move(image), old_layout, new_layout);
}

bool VSL_NAMESPACE::ImageAccessor::copy_by_buffer(CommandManager manager, BufferAccessor* buf, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx) {
    auto copy = command::CopyBufferToArrayImage(*this, buf, layout, target_idx);
    auto phase = manager.start_phase<ComputePhase>();
    phase << copy;
    return true;
}

VSL_NAMESPACE::data_format::Format VSL_NAMESPACE::ImageAccessor::get_format() const {
    if (!_owner)
        throw std::invalid_argument("Image is empty");
    const auto type = static_cast<data_format::FormatTypes>(_owner->format);
    const auto format = data_format::convert(type);
    return format.type() == data_format::FormatTypes::UNDEFINED ? data_format::Format(type, 0) : format;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::ImageAccessor::get_device() const {
    if (!_owner || !_owner->image || !_owner->device)
        throw std::invalid_argument("Image is empty");
    return lifetime::device_from_child(_owner);
}

VSL_NAMESPACE::ImageType VSL_NAMESPACE::ImageAccessor::get_usage() const {
    if (!_owner)
        throw std::invalid_argument("Image is empty");
    return _owner->usage;
}

VSL_NAMESPACE::ResourceUseState& VSL_NAMESPACE::ImageAccessor::use_state() const {
    if (!_owner || !_owner->use)
        throw std::invalid_argument("Image is empty");
    return *_owner->use;
}
VSL_NAMESPACE::ResourceUse VSL_NAMESPACE::ImageAccessor::resource_use() const {
    return lifetime::resource_use_for_image(_owner);
}
