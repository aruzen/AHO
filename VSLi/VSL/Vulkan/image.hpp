//
// Created by morimoto_hibiki on 2025/07/31.
//

#ifndef AHO_ALL_IMAGE_HPP
#define AHO_ALL_IMAGE_HPP
#include "../define.h"
#include "pv.h"

#include "buffer_and_image_accessor.h"
#include "../format.h"
#include "../utils/flags.h"

#include "buffer.h"

namespace VSL_NAMESPACE {
    // memory propertyとformatをtemplateにしたいかも
    template<ImageType Usage, MemoryProperty MemProp = MemoryProperty::DeviceLocal, ImageLayout Layout = ImageLayout::Undefined>
    struct Image : public ImageAccessor {
        Image(LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
              data_format::___Format format = data_format::Srgb8RGBA);

        template<MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode>
        bool copyByBuffer(CommandManager manager, Buffer<MemType, MemProperty, ShareMode> &buffer);
    };
}

template<vsl::ImageType Usage, vsl::MemoryProperty MemProp, vsl::ImageLayout Layout>
vsl::Image<Usage, MemProp, Layout>::Image(vsl::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height,
                                          data_format::___Format format) {
    _data = MakeImage(Usage, MemProp, Layout, device, width, height, format);
}

#endif //AHO_ALL_IMAGE_HPP
