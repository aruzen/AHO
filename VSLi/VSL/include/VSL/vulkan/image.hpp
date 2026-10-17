//
// Created by morimoto_hibiki on 2025/07/31.
//

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
        // Both zero means the full image. Coordinates then must also be zero.
        std::uint32_t width = 0, height = 0;
        std::uint32_t layer = 0;
    };

    // memory propertyとformatをtemplateにしたいかも
    template<ImageType Usage, MemoryProperty MemProp = MemoryProperty::DeviceLocal, ImageLayout Layout = ImageLayout::Undefined>
    struct Image : public ImageAccessor {
        Image(LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
              data_format::___Format format = data_format::Srgb8RGBA, std::uint32_t count = 1, SampleCount samples = SampleCount::One);

        template<MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode>
        bool copyByBuffer(CommandManager manager, Buffer<MemType, MemProperty, ShareMode> &buffer);
    };
}

template<vsl::ImageType Usage, vsl::MemoryProperty MemProp, vsl::ImageLayout Layout>
vsl::Image<Usage, MemProp, Layout>::Image(vsl::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
                                          data_format::___Format format, std::uint32_t count, SampleCount samples) {
    _data = MakeImage(Usage, MemProp, Layout, device, width, height, format, count, samples);
}

#endif //AHO_ALL_IMAGE_HPP
