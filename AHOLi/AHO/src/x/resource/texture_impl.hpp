#pragma once
#include <AHO/x/resource/texture.hpp>

namespace AHO_NAMESPACE::x::resource {
    struct Texture::Data {
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::Sampled | VSL_NAMESPACE::ImageType::TransferDst | VSL_NAMESPACE::ImageType::TransferSrc> image;
        std::uint32_t width, height;
        bool initialized = false;
        VSL_NAMESPACE::Completion update;
        Data(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::___Format format);
        void settle();
    };

    inline Texture::Data::Data(VSL_NAMESPACE::LogicalDeviceAccessor device, std::uint32_t width, std::uint32_t height, VSL_NAMESPACE::data_format::___Format format)
        : image(device, width, height, format), width(width), height(height) {}

    inline void Texture::Data::settle() {
        const auto status = update.status();
        if (status == VSL_NAMESPACE::Completion::Status::Complete) {
            initialized = true;
            update = {};
        } else if (status == VSL_NAMESPACE::Completion::Status::Cancelled)
            update = {};
    }
} // namespace AHO_NAMESPACE::x::resource
