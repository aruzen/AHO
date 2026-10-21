#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
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
    if (!device.data_ || !width || !height || !count || format.type() == data_format::FormatTypes::UNDEFINED)
        throw std::invalid_argument("Invalid image device/extent/format");
    const auto sample_bits = static_cast<std::uint32_t>(samples);
    if (!sample_bits || sample_bits > 64 || (sample_bits & (sample_bits - 1)))
        throw std::invalid_argument("Invalid image sample count");
    auto data_ = std::make_shared<impl::ImageImpl>();
    data_->device = device.data_;
    data_->height = height;
    data_->width = width;
    data_->count = count;
    data_->usage = type;
    data_->samples = samples;
    data_->format = (vk::Format)format.type();

    vk::ImageCreateInfo image_info;
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = data_->format;
    image_info.extent = vk::Extent3D{width, height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = count;
    image_info.samples = static_cast<vk::SampleCountFlagBits>(samples);
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.usage = (vk::ImageUsageFlagBits)type;
    image_info.initialLayout = (vk::ImageLayout)layout;

    auto image = device.data_->device.createImageUnique(image_info);

    vk::MemoryRequirements mem_requirements = device.data_->device.getImageMemoryRequirements(*image);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = VSL_NAMESPACE::impl::helper::find_memory_type(device.data_, mem_requirements.memoryTypeBits,
                                                                             (vk::MemoryPropertyFlagBits)prop);

    data_->memory = device.data_->device.allocateMemory(alloc_info);

    device.data_->device.bindImageMemory(*image, data_->memory, 0);
    data_->image = image.release();

    vk::ImageViewCreateInfo view_info;
    view_info.image = data_->image;
    view_info.viewType = count == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
    view_info.format = (vk::Format)format.type();
    ;
    view_info.subresourceRange.aspectMask = impl::helper::image_aspects(data_->format);
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = count;

    data_->view = device.data_->device.createImageView(view_info);
    return data_;
}

void VSL_NAMESPACE::ImageAccessor::TransformLayout(VSL_NAMESPACE::CommandManager manager, std::shared_ptr<impl::ImageImpl> data,
                                                   VSL_NAMESPACE::ImageLayout old_layout, VSL_NAMESPACE::ImageLayout new_layout) {
    auto phase = manager.start_phase<ComputePhase>();
    phase << command::ChangeImageBarrier(ImageAccessor{std::move(data)}, old_layout, new_layout);
}

bool VSL_NAMESPACE::ImageAccessor::copy_by_buffer(CommandManager manager, BufferAccessor* buf, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx) {
    auto phase = manager.start_phase<ComputePhase>();
    if (this->data_->count == 1)
        phase << command::CopyBufferToImage(*this, buf, layout);
    else
        phase << command::CopyBufferToArrayImage(*this, buf, layout, target_idx);
    return true;
}

VSL_NAMESPACE::impl::ImageImpl::~ImageImpl() {
    if (!device)
        return;
    device->device.destroy(view);
    if (memory != VK_NULL_HANDLE) {
        device->device.destroy(image);
        device->device.free(memory);
    }
}
VSL_NAMESPACE::data_format::Format VSL_NAMESPACE::ImageAccessor::get_format() const {
    const auto type = static_cast<data_format::FormatTypes>(data_->format);
    const auto format = data_format::convert(type);
    return format.type() == data_format::FormatTypes::UNDEFINED ? data_format::Format(type, 0) : format;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::ImageAccessor::get_device() const {
    if (!data_)
        throw std::invalid_argument("Image is empty");
    return {data_->device};
}

VSL_NAMESPACE::ImageType VSL_NAMESPACE::ImageAccessor::get_usage() const {
    if (!data_)
        throw std::invalid_argument("Image is empty");
    return data_->usage;
}
