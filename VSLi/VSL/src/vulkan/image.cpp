#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/image_storage.hpp>
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
    return std::size_t(data_->width) * data_->height * data_->count * convert((data_format::FormatTypes)data_->format).size();
}

std::shared_ptr<VSL_NAMESPACE::impl::ImageImpl>
VSL_NAMESPACE::ImageAccessor::MakeImage(VSL_NAMESPACE::ImageType type,
                                        VSL_NAMESPACE::MemoryProperty prop,
                                        VSL_NAMESPACE::ImageLayout layout,
                                        VSL_NAMESPACE::LogicalDeviceAccessor device,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        VSL_NAMESPACE::data_format::Format format,
                                        std::uint32_t count, SampleCount samples) {
    return lifetime::make_heap_image(device, {type, width, height, format, count, samples, prop, layout});
}

void VSL_NAMESPACE::ImageAccessor::TransformLayout(VSL_NAMESPACE::CommandManager manager, std::shared_ptr<impl::ImageImpl> data,
                                                   VSL_NAMESPACE::ImageLayout old_layout, VSL_NAMESPACE::ImageLayout new_layout) {
    auto phase = manager.start_phase<ComputePhase>();
    phase << command::ChangeImageBarrier(ImageAccessor{std::move(data)}, old_layout, new_layout);
}

bool VSL_NAMESPACE::ImageAccessor::copy_by_buffer(CommandManager manager, BufferAccessor* buf, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx) {
    auto phase = manager.start_phase<ComputePhase>();
    phase << command::CopyBufferToArrayImage(*this, buf, layout, target_idx);
    return true;
}

VSL_NAMESPACE::data_format::Format VSL_NAMESPACE::ImageAccessor::get_format() const {
    const auto type = static_cast<data_format::FormatTypes>(data_->format);
    const auto format = data_format::convert(type);
    return format.type() == data_format::FormatTypes::UNDEFINED ? data_format::Format(type, 0) : format;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::ImageAccessor::get_device() const {
    if (!data_)
        throw std::invalid_argument("Image is empty");
    return {data_->device->shared_from_this()};
}

VSL_NAMESPACE::ImageType VSL_NAMESPACE::ImageAccessor::get_usage() const {
    if (!data_)
        throw std::invalid_argument("Image is empty");
    return data_->usage;
}

VSL_NAMESPACE::ResourceUseState& VSL_NAMESPACE::ImageAccessor::use_state() const {
    if (!data_ || !data_->use)
        throw std::invalid_argument("Image is empty");
    return *data_->use;
}
VSL_NAMESPACE::ResourceUse VSL_NAMESPACE::ImageAccessor::resource_use() const {
    return ResourceUse::ForImage(data_);
}
