//
// Created by morimoto_hibiki on 2025/07/10.
//

#ifndef AHO_ALL_IMAGE_HPP
#define AHO_ALL_IMAGE_HPP

#include "AHO/define.hpp"

#include <filesystem>
#include <optional>

namespace AHO_NAMESPACE::resource {
    class Image {
        struct ImageData {
            unsigned char *buffer = nullptr;
            int channel = 0, width = 0, height = 0;

            ~ImageData();
        };

        std::shared_ptr<ImageData> _data;
    public:
        Image();
#ifdef AHO_STB_IMAGE_ENABLE
        Image(std::filesystem::path path);
#endif // AHO_STB_IMAGE_ENABLE

        std::optional<unsigned char**> data();
    };
}

#endif //AHO_ALL_IMAGE_HPP
