#ifndef VSL_VULKAN_IMAGE_HPP
#define VSL_VULKAN_IMAGE_HPP
#include "../define.hpp"
#include "pv.hpp"

#include "buffer_and_image_accessor.hpp"
#include "../format.hpp"
#include "../utils/flags.hpp"

#include "buffer.hpp"

namespace VSL_NAMESPACE {
    struct ImageRegion {
        std::uint32_t x = 0, y = 0;
        // 幅と高さがともに0なら画像全体を対象にする。その場合は座標も0にする。
        std::uint32_t width = 0, height = 0;
        std::uint32_t layer = 0;
    };

    template<ImageType usage, MemoryProperty mem_prop = MemoryProperty::DEVICE_LOCAL, ImageLayout layout = ImageLayout::UNDEFINED>
    struct Image : public ImageAccessor {
        Image(LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
              data_format::Format format = data_format::SRGB8_RGBA, std::uint32_t count = 1, SampleCount samples = SampleCount::ONE);

        template<MemoryType mem_type, MemoryProperty mem_property, SharingMode share_mode>
        bool copy_by_buffer(CommandManager manager, Buffer<mem_type, mem_property, share_mode>& buffer);
    };
} // namespace VSL_NAMESPACE

template<VSL_NAMESPACE::ImageType usage, VSL_NAMESPACE::MemoryProperty mem_prop, VSL_NAMESPACE::ImageLayout layout>
VSL_NAMESPACE::Image<usage, mem_prop, layout>::Image(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
                                                     data_format::Format format, std::uint32_t count, SampleCount samples) {
    data_ = MakeImage(usage, mem_prop, layout, device, width, height, format, count, samples);
}

#endif
