#ifndef AHO_ALL_IMAGE_HPP
#define AHO_ALL_IMAGE_HPP
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

    template<ImageType Usage, MemoryProperty MemProp = MemoryProperty::DeviceLocal, ImageLayout Layout = ImageLayout::Undefined>
    struct Image : public ImageAccessor {
        Image(LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
              data_format::___Format format = data_format::Srgb8RGBA, std::uint32_t count = 1, SampleCount samples = SampleCount::One);

        template<MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode>
        bool copyByBuffer(CommandManager manager, Buffer<MemType, MemProperty, ShareMode>& buffer);
    };
} // namespace VSL_NAMESPACE

template<VSL_NAMESPACE::ImageType Usage, VSL_NAMESPACE::MemoryProperty MemProp, VSL_NAMESPACE::ImageLayout Layout>
VSL_NAMESPACE::Image<Usage, MemProp, Layout>::Image(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
                                                    data_format::___Format format, std::uint32_t count, SampleCount samples) {
    _data = MakeImage(Usage, MemProp, Layout, device, width, height, format, count, samples);
}

#endif
