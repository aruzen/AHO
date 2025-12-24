//
// Created by morimoto_hibiki on 2025/10/07.
//

#include <AHO/resource/image.hpp>

#ifdef AHO_STB_IMAGE_ENABLE
#define STB_IMAGE_IMPLEMENTATION
#include "../../../thirdparty/stb_image.h"
#endif // AHO_STB_IMAGE_ENABLE

aho::resource::Image::ImageData::~ImageData() {
    delete[] buffer;
}

aho::resource::Image::Image() : _data(nullptr) {}

#ifdef AHO_STB_IMAGE_ENABLE
aho::resource::Image::Image(std::filesystem::path path) {
    _data = std::make_shared<ImageData>();
    if (not stbi_info(path.string().c_str(), &_data->width, &_data->height, &_data->channel))
        _data = nullptr;

    _data->buffer = stbi_load(path.string().c_str(), &_data->width, &_data->height, &_data->channel, STBI_rgb_alpha);

    /*
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
     */
}
#endif // AHO_STB_IMAGE_ENABLE

std::optional<unsigned char**> aho::resource::Image::data() {
    if (not _data)
        return std::nullopt;
    if (_data->buffer == nullptr)
        return std::nullopt;
    return &_data->buffer;
}
